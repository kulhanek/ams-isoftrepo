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
#include <vector>
#include <boost/shared_ptr.hpp>

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================

class CVerRecord {
    public:
    CVerRecord(void);
    CSmallString   version;
    int            verindx;
    bool operator == (const CVerRecord& left) const;
};

//------------------------------------------------------------------------------

CVerRecord::CVerRecord(void)
{
    verindx = 0.0;
}

//------------------------------------------------------------------------------

bool CVerRecord::operator == (const CVerRecord& left) const
{
    bool result = true;
    result &= version == left.version;
    return(result);
}

//------------------------------------------------------------------------------

bool sort_tokens(const CVerRecord& left,const CVerRecord& right)
{
    if( left.version == right.version ) return(true);
    if( left.verindx > right.verindx ) return(true);
    if( left.verindx == right.verindx ){
        return( strcmp(left.version,right.version) > 0);
    }
    return(false);
}

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================

bool CISoftRepoServer::_Module(CFCGIRequest& request)
{
    // parameters ------------------------------------------------------
    CTemplateParams    params;

    params.Initialize();
    params.SetParam("AMSVER",LibBuildVersion_AMS_Web);
    params.Include("MONITORING",GetMonitoringIFrame());

    ProcessCommonParams(request,params);

    // IDs ------------------------------------------
    CSmallString module_name;

    CModUtils::ParseModuleName(request.Params.GetValue("module"),module_name);
    params.SetParam("MODULE",module_name);
    params.SetParam("MODULEURL",CFCGIParams::EncodeString(module_name));

    // populate cache ------------
    CModuleController mod_controller;
    mod_controller.InitModuleControllerConfig(BundleName,BundlePath);
    mod_controller.LoadBundles(EMBC_BIG);
    CModCache mod_cache;
    mod_controller.MergeBundles(mod_cache);

    // get module
    CXMLElement* p_module = mod_cache.GetModule(module_name);
    if( p_module == NULL ) {
        CSmallString error;
        error << "module not found '" << module_name << "'";
        ES_ERROR(error);
        return(false);
    }

    // module versions ---------------------------
    CXMLElement* p_tele = p_module->GetChildElementByPath("builds/build");
    std::list<CVerRecord>   versions;

    while( p_tele != NULL ) {
        CVerRecord verrcd;
        verrcd.verindx = 0.0;
        p_tele->GetAttribute("ver",verrcd.version);
        p_tele->GetAttribute("verindx",verrcd.verindx);
        versions.push_back(verrcd);

        p_tele = p_tele->GetNextSiblingElement("build");
    }

    versions.sort(sort_tokens);
    versions.unique();

    params.StartCycle("VERSIONS");

    std::list<CVerRecord>::iterator it = versions.begin();
    std::list<CVerRecord>::iterator ie = versions.end();

    int count = 0;
    while( it != ie ){
        count++;
        CSmallString full_name;
        full_name = module_name + ":" + (*it).version;
        params.SetParam("MODVER",full_name);
        params.SetParam("MODVERURL",CFCGIParams::EncodeString(full_name));
        if( count > 5 ){
            params.SetParam("CLASS","old");
        } else {
            params.SetParam("CLASS","new");
        }
        params.NextRun();
        it++;
    }
    params.EndCycle("VERSIONS");

    params.StartCondition("SHOWOLD",count > 5 );
    params.EndCondition("SHOWOLD");

    // description --------------------------------
    CXMLElement* p_doc = mod_cache.GetModuleDoc(p_module);
    if( p_doc != NULL ) {
        params.Include("DESCRIPTION",p_doc);
    }

    // default -----------------------------------
    CSmallString defa,defb,dver,darch,dpar;
    CModCache::GetModuleDefaults(p_module,dver,darch,dpar);

    if( darch == NULL ) darch = "auto";
    if( dpar == NULL ) dpar = "auto";

    defa = module_name + ":" + dver;
    defb = darch + ":" + dpar;

    params.SetParam("DEFAULTA",defa);
    params.SetParam("DEFAULTB",defb);

    // acl ---------------------------------------
    bool show_acl = true;
    CSmallString defrule;
    CXMLElement* p_acl = p_module->GetFirstChildElement("acl");
    if( p_acl ) p_acl->GetAttribute("default",defrule);
    if( (defrule == NULL) || (defrule == "allow") ){
        if( p_acl ){
            show_acl = p_acl->GetNumberOfChildNodes() != 0;
        } else {
            show_acl = false;
        }
    } else {
        show_acl = true;
    }

    params.StartCondition("ACL",show_acl);
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
    if( p_acl ) p_acl->GetAttribute("default",defrule);
    if( (defrule == NULL) || (defrule == "allow") ){
        params.SetParam("DEFACL","allow all");
    } else {
        params.SetParam("DEFACL","deny all");
    }
    bool any_acl_for_build = false;
    CXMLElement* p_build;

    p_build = p_module->GetChildElementByPath("builds/build");
    while( p_build != NULL ){
        if( p_build->GetFirstChildElement("acl") != NULL ){
            any_acl_for_build = true;
            break;
        }
        p_build = p_build->GetNextSiblingElement("build");
    }

    params.StartCondition("EXTRAACL",any_acl_for_build);
    p_build = p_module->GetChildElementByPath("builds/build");
    params.StartCycle("EBUILDS");
    while( p_build != NULL ){
        CSmallString lver,larch,lmode;
        p_build->GetAttribute("ver",lver);
        p_build->GetAttribute("arch",larch);
        p_build->GetAttribute("mode",lmode);
        CSmallString full_name;
        full_name = module_name + ":" + lver + ":" + larch + ":" + lmode;

        if( p_build->GetFirstChildElement("acl") != NULL ){
            params.SetParam("BUILD",full_name);
            params.NextRun();
            break;
        }
        p_build = p_build->GetNextSiblingElement("build");
    }
    params.EndCycle("EBUILDS");
    params.EndCondition("EXTRAACL");
    params.EndCondition("ACL");

    // dependencies ------------------------------
    CXMLElement* p_deps = p_module->GetFirstChildElement("deps");
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

            params.StartCondition("MVER",mver != NULL);
                params.SetParam("DNAME",mname);
                params.SetParam("DNAMEURL",CFCGIParams::EncodeString(mname));
                params.SetParam("DVER",mver);
                params.SetParam("DVERURL",CFCGIParams::EncodeString(mver));
            params.EndCondition("MVER");

            params.StartCondition("MBUILD",mmode != NULL);
                params.SetParam("DNAME",mmode);
                params.SetParam("DNAMEURL",CFCGIParams::EncodeString(mmode));
                params.SetParam("DVER",mmode);
                params.SetParam("DVERURL",CFCGIParams::EncodeString(mmode));
                params.SetParam("DARCH",mmode);
                params.SetParam("DARCHURL",CFCGIParams::EncodeString(mmode));
                params.SetParam("DMODE",mmode);
                params.SetParam("DMODEURL",CFCGIParams::EncodeString(mmode));
            params.EndCondition("MBUILD");

            params.NextRun();
            p_dep = p_dep->GetNextSiblingElement("dep");
        }
        params.EndCycle("DEPS");
    }
    params.EndCondition("DEPENDENCIES");

    if( params.Finalize() == false ) {
        ES_ERROR("unable to prepare parameters");
        return(false);
    }

    // process template ------------------------------------------------
    bool result = ProcessTemplate(request,"Module.html",params);

    return(result);
}

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================
