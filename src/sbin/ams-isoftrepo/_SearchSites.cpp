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

class CFoundModule {
public:
    CSmallString                Name;
    std::vector<CSmallString>   Sites;
};

//------------------------------------------------------------------------------

// declared in _ListSites.cpp
bool SiteNameCompare(CSite* const &left,CSite* const &right );

//------------------------------------------------------------------------------

bool FoundNameCompare(const CFoundModule &left,const CFoundModule &right )
{
    return( strcmp(left.Name,right.Name) < 0 );
}

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================

bool CISoftRepoServer::_SearchSites(CFCGIRequest& request)
{
    // parameters ------------------------------------------------------
    CTemplateParams    params;

    params.Initialize();
    params.SetParam("AMSVER",LibBuildVersion_AMS_Web);
    params.Include("MONITORING",GetMonitoringIFrame());

    ProcessCommonParams(request,params);

    CSmallString search_string = request.Params.GetValue("search");
    params.SetParam("SEARCH",search_string);

    //IDs ------------------------------------------

    // make list of all available sites -------------
    CDirectoryEnum         dir_enum(AMSGlobalConfig.GetAMSRootDir() / "etc" / "sites");
    CFileName              site_sid;
    std::vector<CSite*>    sites;

    dir_enum.StartFindFile("*");

    while( dir_enum.FindFile(site_sid) ) {
        CAmsUUID    site_id;
        if( site_id.LoadFromString(site_sid) == false ) continue;
        CSite* p_site;
        try {
            p_site = new CSite;
        } catch(...) {
            ES_ERROR("unable to allocate memory");
            for(unsigned int i=0; i < sites.size(); i++) {
                delete sites[i];
            }
            return(false);
        }
        if( p_site->LoadConfig(site_sid) == false ) {
            continue;
        }
        sites.push_back(p_site);
    }
    dir_enum.EndFindFile();

    // sort sites
    std::sort(sites.begin(),sites.end(),SiteNameCompare);

    std::vector<CFoundModule> hints;

    // search for every site
    for(unsigned int i=0; i < sites.size(); i++) {
        // make list of all available categories and modules ------------
        CSmallString site_sid;
        site_sid = CUtils::GetSiteID(sites[i]->GetName());

        if( site_sid == NULL ) {
            ES_ERROR("site UUID was not found");
            for(unsigned int i=0; i < sites.size(); i++) {
                delete sites[i];
            }
            return(false);
        }

        AMSGlobalConfig.SetActiveSiteID(site_sid);

        // initialze AMS cache
        if( Cache.LoadCache(true) == false) {
            ES_ERROR("unable to load AMS cache");
            for(unsigned int i=0; i < sites.size(); i++) {
                delete sites[i];
            }
            return(false);
        }

        std::vector<CXMLElement*> results;

        //search in cache
        Cache.SearchCache(search_string+"*",results);

        // merge hints
        for(unsigned int j=0; j < results.size(); j++) {
            CSmallString name;
            results[j]->GetAttribute("name",name);
            unsigned int k=0;
            for(; k < hints.size(); k++) {
                if( hints[k].Name == name ) break;
            }

            if( k < hints.size() ) {
                hints[k].Sites.push_back(sites[i]->GetName());
            } else {
                CFoundModule data;
                data.Name = name;
                data.Sites.push_back(sites[i]->GetName());
                hints.push_back(data);
            }
        }
    }

    std::sort(hints.begin(),hints.end(),FoundNameCompare);

    // print results
    params.StartCycle("RESULTS");
    for(unsigned int i=0; i < hints.size(); i++) {
        params.SetParam("MODULE",hints[i].Name);
        params.SetParam("MODULEURL",CFCGIParams::EncodeString(hints[i].Name));
        params.StartCycle("SITES");
        for(unsigned int j=0; j < hints[i].Sites.size(); j++) {
            params.SetParam("SITE",hints[i].Sites[j]);
            params.SetParam("SEP","");
            if( j + 1 < hints[i].Sites.size() ) params.SetParam("SEP",",");
            params.NextRun();
        }
        params.EndCycle("SITES");
        params.NextRun();
    }
    params.EndCycle("RESULTS");

    // release data
    for(unsigned int i=0; i < sites.size(); i++) {
        delete sites[i];
    }

    if( params.Finalize() == false ) {
        ES_ERROR("unable to prepare parameters");
        return(false);
    }

    //process template ------------------------------------------------
    bool result;
    result = ProcessTemplate(request,"SearchSites.html",params);

    return(result);
}

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================
