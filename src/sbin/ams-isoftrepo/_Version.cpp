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

using namespace std;

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================

bool SortBuildsByMode(const CSmallString& left,const CSmallString& right)
{
    CSmallString left_arch = CUtils::GetModuleArch(left);
    CSmallString right_arch = CUtils::GetModuleArch(right);
    if( strcmp(left_arch,right_arch) < 0 ) return(true);
    if( strcmp(left_arch,right_arch) > 0 ) return(false);

    CSmallString left_mode = CUtils::GetModuleMode(left);
    CSmallString right_mode = CUtils::GetModuleMode(right);
    return( strcmp(left_mode,right_mode) <= 0 );
}

//------------------------------------------------------------------------------

bool CISoftRepoServer::_Version(CFCGIRequest& request)
{
    // parameters ------------------------------------------------------
    CTemplateParams    params;

    params.Initialize();
    params.SetParam("AMSVER",LibBuildVersion_AMS_Web);
    params.Include("MONITORING",GetMonitoringIFrame());

    ProcessCommonParams(request,params);

    // IDs ------------------------------------------
    CSmallString site_name;
    CSmallString module_name,module_ver,modver;

    site_name = request.Params.GetValue("site");
    if( site_name == NULL ) {
        ES_ERROR("site name is not provided");
        return(false);         // site name has to be provided
    }

    CUtils::ParseModuleName(request.Params.GetValue("module"),module_name,module_ver);
    modver = module_name + ":" + module_ver;

    params.SetParam("SITE",site_name);
    params.SetParam("MODVER",modver);
    params.SetParam("MODVERURL",CFCGIParams::EncodeString(modver));
    params.SetParam("MODULE",module_name);
    params.SetParam("MODULEURL",CFCGIParams::EncodeString(module_name));
    params.SetParam("VERSION",module_ver);

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

    // list of builds ----------------------------
    CSmallString name;
    p_module->GetAttribute("name",name);

    CXMLElement*            p_rels = p_module->GetFirstChildElement("builds");
    CXMLElement*            p_tele;
    CXMLIterator            K(p_rels);

    std::list<CSmallString>   builds;

    while( (p_tele = K.GetNextChildElement("build")) != NULL ) {
        CSmallString lver,larch,lmode;
        p_tele->GetAttribute("ver",lver);
        p_tele->GetAttribute("arch",larch);
        p_tele->GetAttribute("mode",lmode);

        if( module_ver == lver ) {
            CSmallString full_name;
            full_name = module_name + ":" + lver + ":" + larch + ":" + lmode;
            builds.push_back(full_name);
        }
    }

    builds.sort(SortBuildsByMode);

    std::list<CSmallString>::iterator   it = builds.begin();
    std::list<CSmallString>::iterator   ie = builds.end();

    params.StartCycle("BUILDS");

    while( it != ie ){
        CSmallString full_name = (*it);
        params.SetParam("BUILD",full_name);
        params.SetParam("TBUILD",CFCGIParams::EncodeString(full_name));
        params.NextRun();
        it++;
    }
    params.EndCycle("BUILDS");

    if( params.Finalize() == false ) {
        ES_ERROR("unable to prepare parameters");
        return(false);
    }

    // process template ------------------------------------------------
    bool result = ProcessTemplate(request,"Version.html",params);

    return(result);
}

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================
