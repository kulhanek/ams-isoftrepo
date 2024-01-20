// =============================================================================
//  AMS - Advanced Module System
// -----------------------------------------------------------------------------
//     Copyright (C) 2012 Petr Kulhanek (kulhanek@chemi.muni.cz)
//     Copyright (C) 2011      Petr Kulhanek, kulhanek@chemi.muni.cz
//     Copyright (C) 2001-2008 Petr Kulhanek, kulhanek@chemi.muni.cz
//
//     This program is free software; you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation; either version 2 of the License, or
//     (at your option) any later version.
//
//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
//
//     You should have received a copy of the GNU General Public License along
//     with this program; if not, write to the Free Software Foundation, Inc.,
//     51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
// =============================================================================

#include "ISoftRepoServer.hpp"
#include <TemplateParams.hpp>
#include <ErrorSystem.hpp>
#include <ModCache.hpp>
#include <ModUtils.hpp>
#include <ModuleController.hpp>

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================

bool CISoftRepoServer::_Build(CFCGIRequest& request)
{
    // parameters ------------------------------------------------------
    CTemplateParams    params;

    params.Initialize();
    params.SetParam("AMSVER",LibBuildVersion_AMS_Web);
    params.Include("MONITORING",GetMonitoringIFrame());

    ProcessCommonParams(request,params);

    // IDs ------------------------------------------
    CSmallString module_name,module_ver,module_arch,module_mode,modver,build;
    CSmallString module = request.Params.GetValue("module");

    CModUtils::ParseModuleName(module,module_name,module_ver,module_arch,module_mode);
    modver = module_name + ":" + module_ver;
    build = module_name + ":" + module_ver + ":" + module_arch + ":" + module_mode;

    // populate cache ------------
    CModuleController mod_controller;
    mod_controller.InitModuleControllerConfig();
    mod_controller.LoadBundles(EMBC_SMALL);
    CModCache mod_cache;
    mod_controller.MergeBundles(mod_cache);

    CXMLElement* p_module = mod_cache.GetModule(module_name);
    if( p_module == NULL ) {
        CSmallString error;
        error << "module not found '" << module_name << "'";
        ES_ERROR(error);
        return(false);
    }

    CXMLElement* p_build = CModCache::GetBuild(p_module,module_ver,module_arch,module_mode);
    if( p_build == NULL ) {
        CSmallString error;
        error << "build '" << module << "' was not found";
        ES_ERROR(error);
        return(false);
    }

    params.SetParam("MODVER",modver);
    params.SetParam("MODVERURL",CFCGIParams::EncodeString(modver));
    params.SetParam("BUILD",build);
    params.SetParam("MODULE",module_name);
    params.SetParam("MODULEURL",CFCGIParams::EncodeString(module_name));
    params.SetParam("VERSION",module_ver);
    params.SetParam("ARCH",module_arch);
    params.SetParam("MODE",module_mode);

    // acl ---------------------------------------
    CXMLElement* p_acl = p_build->GetFirstChildElement("acl");
    params.StartCondition("ACL",p_acl != NULL);
    if( p_acl != NULL ){
        params.StartCycle("RULES");
        CXMLElement* p_rule = p_acl->GetFirstChildElement();
        while( p_rule != NULL ){
            CSmallString rule = p_rule->GetName();
            CSmallString group;
            p_rule->GetAttribute("group",group);
            params.SetParam("ACLRULE",rule + " " + group);
            params.NextRun();
            p_rule = p_rule->GetNextSiblingElement();
        }
        params.EndCycle("RULES");
    }
    CSmallString defrule;
    if( p_acl ) p_acl->GetAttribute("default",defrule);
    if( (defrule == NULL) || (defrule == "allow") ){
        params.SetParam("DEFACL","allow all");
    } else {
        params.SetParam("DEFACL","deny all");
    }
    params.EndCondition("ACL");

    // dependencies ------------------------------
    CXMLElement* p_deps = p_build->GetFirstChildElement("deps");
    params.StartCondition("DEPENDENCIES",p_deps != NULL);
    if( p_deps != NULL ){
        params.StartCycle("DEPS");
        CXMLElement* p_dep = p_deps->GetFirstChildElement("dep");
        while( p_dep != NULL ){
            CSmallString module;
            CSmallString type;
            p_dep->GetAttribute("name",module);
            p_dep->GetAttribute("type",type);

            params.SetParam("DTYPE",type);
            CSmallString mname,mver,march,mmode;
            CModUtils::ParseModuleName(module,mname,mver,march,mmode);

            params.StartCondition("MNAM",mver == NULL);
                params.SetParam("DNAME",mname);
                params.SetParam("DNAMEURL",CFCGIParams::EncodeString(mname));
            params.EndCondition("MNAM");

            params.StartCondition("MVER",(mver != NULL) && (mmode == NULL));
                params.SetParam("DNAME",mname);
                params.SetParam("DNAMEURL",CFCGIParams::EncodeString(mname));
                params.SetParam("DVER",mver);
                params.SetParam("DVERURL",CFCGIParams::EncodeString(mver));
            params.EndCondition("MVER");

            params.StartCondition("MBUILD",mmode != NULL);
                params.SetParam("DNAME",mname);
                params.SetParam("DNAMEURL",CFCGIParams::EncodeString(mname));
                params.SetParam("DVER",mver);
                params.SetParam("DVERURL",CFCGIParams::EncodeString(mver));
                params.SetParam("DARCH",march);
                params.SetParam("DARCHURL",CFCGIParams::EncodeString(march));
                params.SetParam("DMODE",mmode);
                params.SetParam("DMODEURL",CFCGIParams::EncodeString(mmode));
            params.EndCondition("MBUILD");

            params.NextRun();
            p_dep = p_dep->GetNextSiblingElement("dep");
        }
        params.EndCycle("DEPS");
    }
    params.EndCondition("DEPENDENCIES");

    // technical specification -------------------
    CXMLElement*    p_sele = NULL;
    if( p_build != NULL ) p_sele = p_build->GetChildElementByPath("setup/item");

    params.StartCycle("T");

    while( p_sele != NULL ) {
        params.SetParam("TTYPE",p_sele->GetName());
        CSmallString name;
        CSmallString value;
        CSmallString operation;
        CSmallString priority;
        bool         secret = false;
        if( p_sele->GetName() == "variable" ) {
            p_sele->GetAttribute("name",name);
            p_sele->GetAttribute("value",value);
            p_sele->GetAttribute("operation",operation);
            p_sele->GetAttribute("priority",priority);
            p_sele->GetAttribute("secret",secret);
        }
        if( p_sele->GetName() == "script" ) {
            p_sele->GetAttribute("name",name);
            p_sele->GetAttribute("type",operation);
            p_sele->GetAttribute("priority",priority);
        }
        if( p_sele->GetName() == "alias" ) {
            p_sele->GetAttribute("name",name);
            p_sele->GetAttribute("value",value);
            p_sele->GetAttribute("priority",priority);
        }
        if( secret ){
            value = "*******";
        }
        params.SetParam("TNAME",name);
        params.SetParam("TVALUE",value);
        params.SetParam("TOPERATION",operation);
        params.SetParam("TPRIORITY",priority);
        params.NextRun();
        // FIXME
        p_sele = p_sele->GetNextSiblingElement("item");
    }
    params.EndCycle("T");

    if( params.Finalize() == false ) {
        ES_ERROR("unable to prepare parameters");
        return(false);
    }

    // process template ------------------------------------------------
    bool result = ProcessTemplate(request,"Build.html",params);

    return(result);
}

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================
