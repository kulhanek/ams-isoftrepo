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
#include <vector>
#include <algorithm>
#include <list>
#include <FileSystem.hpp>
#include <XMLParser.hpp>
#include <XMLPrinter.hpp>
#include <XMLElement.hpp>
#include <AMSGlobalConfig.hpp>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/join.hpp>

//------------------------------------------------------------------------------

using namespace std;
using namespace boost;
using namespace boost::algorithm;

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================

bool SiteNameCompare(CSite* const &left,CSite* const &right )
{
    if( left->GetGroupDesc() != right->GetGroupDesc() ) {
        return( strcmp(left->GetGroupDesc(),right->GetGroupDesc()) < 0 );
    }

    return( strcmp(left->GetName(),right->GetName()) < 0 );
}

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================

bool CISoftRepoServer::_ListSites(CFCGIRequest& request)
{
    // parameters ------------------------------------------------------
    CTemplateParams    params;

    params.Initialize();
    params.SetParam("AMSVER",LibBuildVersion_AMS_Web);
    params.Include("MONITORING",GetMonitoringIFrame());

    ProcessCommonParams(request,params);

    // make list of all available sites -------------
    CDirectoryEnum         dir_enum(AMSGlobalConfig.GetAMSRootDir() / "etc" / "sites");
    CFileName              site_sid;
    std::vector<CSite*>    sites;

    dir_enum.StartFindFile("{*}");

    while( dir_enum.FindFile(site_sid) ) {
        CAmsUUID    site_id;
        if( site_id.LoadFromString(site_sid) == false ) continue;
        CSite* p_site;
        try {
            p_site = new CSite;
        } catch(...) {
            ES_ERROR("unable to allocate memory");
            return(false);
        }
        if( p_site->LoadConfig(site_sid) == false ) {
            delete p_site;
            continue;
        }
        if( p_site->IsSiteVisible() == false ) {
            delete p_site;
            continue;
        }
        sites.push_back(p_site);
    }
    dir_enum.EndFindFile();

    if( sites.size() > 0 ) {
        // sort sizes
        std::sort(sites.begin(),sites.end(),SiteNameCompare);

        // populate template
        CSmallString last_group;
        bool         opened = false;
        params.StartCycle("GROUPS");
        for(unsigned int i=0; i < sites.size(); i++) {
            if( last_group != sites[i]->GetGroupDesc() || (i == 0) ) {
                if( i > 1 ) {
                    params.EndCycle("SITES");
                    params.NextRun();
                }
                last_group = sites[i]->GetGroupDesc();
                params.SetParam("GROUP",sites[i]->GetGroupDesc());
                params.StartCycle("SITES");
                opened = true;
            }
            params.SetParam("SITE",sites[i]->GetName());
            params.NextRun();
        }
        if( opened ) params.EndCycle("SITES");
        params.NextRun();
        params.EndCycle("GROUPS");
    }

    if( params.Finalize() == false ) {
        ES_ERROR("unable to prepare parameters");
        for(unsigned int i=0; i < sites.size(); i++) {
            delete sites[i];
        }
        return(false);
    }

    // process template ------------------------------------------------
    bool result = ProcessTemplate(request,"ListSites.html",params);

    for(unsigned int i=0; i < sites.size(); i++) {
        delete sites[i];
    }

    return(result);
}

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================
