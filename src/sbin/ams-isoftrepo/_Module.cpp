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
#include <vector>
#include <XMLIterator.hpp>
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

//------------------------------------------------------------------------------

typedef boost::shared_ptr<CSite> CSitePtr;

bool SiteOnlyNameCompare(const CSitePtr& left,const CSitePtr& right)
{
   return( strcmp(left->GetName(),right->GetName()) < 0 );
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
    CSmallString site_name;
    CSmallString module_name;

    site_name = request.Params.GetValue("site");
    if( site_name == NULL ) {
        ES_ERROR("site name is not provided");
        return(false);         // site name has to be provided
    }

    CUtils::ParseModuleName(request.Params.GetValue("module"),module_name);

    params.SetParam("SITE",site_name);
    params.SetParam("MODULE",module_name);
    params.SetParam("MODULEURL",CFCGIParams::EncodeString(module_name));

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
        CSmallString error;
        error << "module not found '" << module_name << "'";
        ES_ERROR(error);
        return(false);
    }

    // module versions ---------------------------
    CXMLElement*            p_rels = p_module->GetFirstChildElement("builds");
    CXMLElement*            p_tele;
    CXMLIterator            K(p_rels);
    std::list<CVerRecord>   versions;

    while( (p_tele = K.GetNextChildElement("build")) != NULL ) {
        CVerRecord verrcd;
        verrcd.verindx = 0.0;
        p_tele->GetAttribute("ver",verrcd.version);
        p_tele->GetAttribute("verindx",verrcd.verindx);
        versions.push_back(verrcd);
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
    CXMLElement* p_doc = Cache.GetModuleDescription(p_module);
    if( p_doc != NULL ) {
        params.Include("DESCRIPTION",p_doc);
    }

    // default -----------------------------------
    CSmallString defa,defb,dver,darch,dpar;
    Cache.GetModuleDefaults(p_module,dver,darch,dpar);

    if( darch == NULL ) darch = "auto";
    if( dpar == NULL ) dpar = "auto";

    defa = module_name + ":" + dver;
    defb = darch + ":" + dpar;

    params.SetParam("DEFAULTA",defa);
    params.SetParam("DEFAULTB",defb);

    // module sites ------------------------------
    // make list of all available sites -------------
    CDirectoryEnum         dir_enum(AMSGlobalConfig.GetAMSRootDir() / "etc" / "sites");
    CFileName              lsite_sid;
    std::list<CSitePtr>    sites;

    dir_enum.StartFindFile("{*}");

    while( dir_enum.FindFile(lsite_sid) ) {
        CAmsUUID    site_id;
        if( site_id.LoadFromString(lsite_sid) == false ) continue;
        CSitePtr p_site(new CSite);
        if( p_site->LoadConfig(lsite_sid) == false ) {
            continue;
        }
        if( p_site->IsSiteVisible() == false ) {
            continue;
        }
        sites.push_back(p_site);
    }
    dir_enum.EndFindFile();

    sites.sort(SiteOnlyNameCompare);

    // keep only thos site, where the module is defined
    std::list<CSitePtr>::iterator sit = sites.begin();
    std::list<CSitePtr>::iterator sie = sites.end();

    while( sit != sie ){
        CSitePtr p_site = *sit;
        CCache lcache;
        lcache.LoadCache(p_site->GetID());
        if( lcache.GetModule(module_name) != NULL ){
            sit++; // keep
        } else {
            sit = sites.erase(sit); //remove
        }
    }

    sit = sites.begin();
    sie = sites.end();

    params.StartCycle("MSITES");
    while( sit != sie ){
        CSitePtr p_site = *sit;
        params.SetParam("MSITE",p_site->GetName());
        params.NextRun();
        sit++;
    }
    params.EndCycle("MSITES");

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
            CUtils::ParseModuleName(module,mname,mver,march,mmode);

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
