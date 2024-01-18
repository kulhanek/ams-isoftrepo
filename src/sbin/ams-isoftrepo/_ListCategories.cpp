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
#include <AmsUUID.hpp>
#include <Site.hpp>
#include <ModCache.hpp>
#include <PrintEngine.hpp>
#include <Utils.hpp>

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================

bool CISoftRepoServer::_ListCategories(CFCGIRequest& request)
{
    // parameters ------------------------------------------------------
    CTemplateParams    params;

    params.Initialize();
    params.SetParam("AMSVER",LibBuildVersion_AMS_Web);
    params.Include("MONITORING",GetMonitoringIFrame());

    ProcessCommonParams(request,params);

    // IDs ------------------------------------------
    CSmallString site_name;
    site_name = request.Params.GetValue("site");
    if( site_name == NULL ) {
        ES_ERROR("site name is not provided");
        return(false);         // site name has to be provided
    }

    params.SetParam("SITE",site_name);

    // make list of all available categories and modules ------------
    CSmallString site_sid;
    site_sid = CUtils::GetSiteID(site_name);

    if( site_sid == NULL ) {
        ES_ERROR("site UUID was not found");
        return(false);
    }

    AMSGlobalConfig.SetActiveSiteID(site_sid);

    // initialze AMS cache
    if( Cache.LoadCache() == false) {
        ES_ERROR("unable to load AMS cache");
        return(false);
    }

    // initialze AMS print engine
    if( PrintEngine.LoadConfig() == false) {
        ES_ERROR("unable to load print engine config");
        return(false);
    }

    CSmallString tmp;
    bool include_vers;
    tmp = request.Params.GetValue("include_vers");
    include_vers = tmp == "true";

    params.StartCondition("CHECKED_VERS",include_vers);
    params.EndCondition("CHECKED_VERS");

    if( include_vers ) {
        params.SetParam("ACTION",CSmallString("version"));
    } else {
        params.SetParam("ACTION",CSmallString("module"));
    }

    PrintEngine.ListModAvailableModules(params,include_vers);

    if( params.Finalize() == false ) {
        ES_ERROR("unable to prepare parameters");
        return(false);
    }

    // process template ------------------------------------------------
    bool result = ProcessTemplate(request,"ListCategories.html",params);

    return(result);
}

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================
