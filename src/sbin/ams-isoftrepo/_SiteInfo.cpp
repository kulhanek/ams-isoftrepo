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

bool CISoftRepoServer::_SiteInfo(CFCGIRequest& request)
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

    params.SetParam("SITEID",site_sid);

    if( site_sid == NULL ) {
        ES_ERROR("site UUID was not found");
        return(false);
    }

    CSite site;

    if( site.LoadConfig(site_sid) == false ) {
        ES_ERROR("unable to load site");
        return(false);
    }

    params.StartCondition("OWNER",site.GetOrganizationName() != NULL);
        params.SetParam("ORGANIZATION",site.GetOrganizationName());
        params.SetParam("ORGURL",site.GetOrganizationURL());
    params.EndCondition("OWNER");

    bool inc_supp = (site.GetSupportEMail() != NULL) || (site.GetDocumentationURL() != NULL);
    params.StartCondition("SUPPORT",inc_supp);

        params.StartCondition("SCONT",site.GetSupportEMail() != NULL);
        params.SetParam("SCONTACTDESC",site.GetSupportName());
        params.SetParam("SCONTACT",site.GetSupportEMail());
        params.EndCondition("SCONT");

        params.StartCondition("SDOC",site.GetDocumentationURL() != NULL);
        params.SetParam("SDOCURL",site.GetDocumentationURL());
        params.SetParam("SDOCTEXT",site.GetDocumentationText());
        params.EndCondition("SDOC");

    params.EndCondition("SUPPORT");

    params.StartCondition("DESCR",site.GetSiteDescrXML() != NULL);
    if( site.GetSiteDescrXML() != NULL ){
        params.Include("DESCRIPTION",site.GetSiteDescrXML());
    }
    params.EndCondition("DESCR");

    params.StartCycle("MOD");

    CXMLIterator    I(site.GetAutoloadedModules());
    CXMLElement*    p_mod;

    p_mod = I.GetNextChildElement("module");

    while( p_mod != NULL ) {
        CSmallString mod_name;
        bool         mod_exp = false;

        p_mod->GetAttribute("name",mod_name);
        p_mod->GetAttribute("export",mod_exp);

        params.SetParam("MOD",mod_name);

        CSmallString name,ver,arch,par;

        CUtils::ParseModuleName(mod_name,name,ver,arch,par);

        CSmallString action;
        action = "realization";

        if( par == NULL ) action = "version";
        if( arch == NULL ) action = "version";
        if( ver == NULL ) action = "module";

        params.SetParam("ACTION",action);

        CSmallString sep;

        if( mod_exp ) {
            sep = "*";
        }

        p_mod = I.GetNextChildElement("module");
        if( p_mod != NULL ) {
            sep += ",";
        }

        params.SetParam("SEP",sep);
        params.NextRun();
    }
    params.EndCycle("MOD");

    if( params.Finalize() == false ) {
        ES_ERROR("unable to prepare parameters");
        return(false);
    }

    // process template ------------------------------------------------
    bool result = ProcessTemplate(request,"SiteInfo.html",params);

    return(result);
}

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================
