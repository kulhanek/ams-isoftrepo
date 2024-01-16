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
#include <Utils.hpp>
#include <AMSGlobalConfig.hpp>
#include <vector>
#include <algorithm>

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================

bool SortModuleHist(CXMLElement* const &left,CXMLElement* const &right)
{
    CSmallString lname,rname;

    left->GetAttribute("name",lname);
    right->GetAttribute("name",rname);

    return( strcmp(lname,rname) < 0 );
}

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================

bool CISoftRepoServer::_SearchSite(CFCGIRequest& request)
{
    // parameters ------------------------------------------------------
    CTemplateParams    params;

    params.Initialize();
    params.SetParam("AMSVER",LibBuildVersion_AMS_Web);
    params.Include("MONITORING",GetMonitoringIFrame());

    ProcessCommonParams(request,params);

    CSmallString search_string = request.Params.GetValue("search");
    params.SetParam("SEARCH",search_string);

    // IDs ------------------------------------------
    CSmallString site_name;
    site_name = request.Params.GetValue("site");

    if( site_name == NULL ) {
        ES_ERROR("site is NULL");
        return(false);
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
    if( Cache.LoadCache(true) == false) {
        ES_ERROR("unable to load AMS cache");
        return(false);
    }

    std::vector<CXMLElement*>  hits;

    // search in cache
    Cache.SearchCache(search_string+"*",hits);

    // how many records
    if( hits.size() == 1 ) {
        CSmallString name;
        hits[0]->GetAttribute("name",name);
        request.Params.SetParam("module",CFCGIParams::EncodeString(name));
        return(_Module(request));
    }

    // sort them
    std::sort(hits.begin(),hits.end(),SortModuleHist);

    // print them
    params.StartCycle("RESULTS");
    for(unsigned int i=0; i < hits.size(); i++) {
        CSmallString name;
        hits[i]->GetAttribute("name",name);
        params.SetParam("MODULE",name);
        params.SetParam("MODULEURL",CFCGIParams::EncodeString(name));
        if( Cache.GetModuleDescription(hits[i]) != NULL ) {
            params.Include("DESCRIPTION",Cache.GetModuleDescription(hits[i]));
        }
        params.NextRun();
    }
    params.EndCycle("RESULTS");

    // unlock - none statement if it is valid
    params.StartCondition("NONE",hits.size() == 0);
    params.EndCondition("NONE");

    if( params.Finalize() == false ) {
        ES_ERROR("unable to prepare parameters");
        return(false);
    }

    // process template ------------------------------------------------
    bool result;
    result = ProcessTemplate(request,"SearchSite.html",params);

    return(result);
}

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================
