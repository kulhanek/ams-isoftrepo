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
#include <DirectoryEnum.hpp>
#include "prefix.h"
#include <AmsUUID.hpp>
#include <Site.hpp>
#include <Cache.hpp>
#include <PrintEngine.hpp>
#include <Utils.hpp>
#include <AMSGlobalConfig.hpp>
#include <XMLIterator.hpp>

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
    CSmallString site_name;
    CSmallString module_name,module_ver,module_arch,module_mode,modver,build;

    CSmallString module = request.Params.GetValue("module");

    CUtils::ParseModuleName(module,module_name,module_ver,module_arch,module_mode);
    modver = module_name + ":" + module_ver;
    build = module_name + ":" + module_ver + ":" + module_arch + ":" + module_mode;

    site_name = request.Params.GetValue("site");
    if( site_name == NULL ) {
        ES_ERROR("site name is not provided");
        return(false);         // site name has to be provided
    }

    // populate cache ------------
    CSmallString site_sid;
    site_sid = CUtils::GetSiteID(site_name);

    if( site_sid == NULL ) {
        ES_ERROR("site UUID was not found");
        return(false);
    }

    AMSGlobalConfig.SetActiveSiteID(site_sid);

    // initialze AMS cache
    if( Cache.LoadCache(true) == false) {
        ES_ERROR("unable to load AMS cache");
        return(false);
    }

    // get module
    CXMLElement* p_module = Cache.GetModule(module_name);
    if( p_module == NULL ) {
        ES_ERROR("module record was not found");
        return(false);
    }

    CXMLElement* p_build = Cache.GetBuild(p_module,module_ver,module_arch,module_mode);
    if( p_build == NULL ) {
        CSmallString error;
        error << "build '" << module << "' was not found";
        ES_ERROR(error);
        return(false);
    }

    params.SetParam("SITE",site_name);
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
            CUtils::ParseModuleName(module,mname,mver,march,mmode);

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
    CXMLElement*    p_setup = NULL;
    if( p_build != NULL ) p_setup = p_build->GetFirstChildElement("setup");

    CXMLIterator    I(p_setup);
    CXMLElement*    p_sele;

    params.StartCycle("T");

    while( (p_sele = I.GetNextChildElement()) != NULL ) {
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
