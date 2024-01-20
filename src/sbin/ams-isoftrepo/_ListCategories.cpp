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

bool CISoftRepoServer::_ListCategories(CFCGIRequest& request)
{
    // parameters ------------------------------------------------------
    CTemplateParams    params;

    params.Initialize();
    params.SetParam("AMSVER",LibBuildVersion_AMS_Web);
    params.Include("MONITORING",GetMonitoringIFrame());

    ProcessCommonParams(request,params);

    // populate cache ------------
    CModuleController mod_controller;
    mod_controller.InitModuleControllerConfig(BundleName,BundlePath);
    mod_controller.LoadBundles(EMBC_SMALL);
    CModCache mod_cache;
    mod_controller.MergeBundles(mod_cache);

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

// get categories
    std::list<CSmallString> cats;
    mod_cache.GetCategories(cats);
    cats.sort();
    cats.unique();

    params.StartCycle("CATEGORIES");

// print modules
    for(CSmallString cat : cats){
        std::list<CSmallString> mods;
        mod_cache.GetModules(cat,mods,include_vers);
        mods.sort();
        mods.unique();
        if( mods.empty() ) continue;

        params.SetParam("CATEGORY",cat);

        params.StartCycle("MODULES");

        for(CSmallString mod : mods){
            params.SetParam("MODULE",mod);
            params.SetParam("MODULEURL",CFCGIParams::EncodeString(mod));
            params.NextRun();
        }

        params.EndCycle("MODULES");
        params.NextRun();
    }

    std::list<CSmallString> mods;
    mod_cache.GetModules("sys",mods,include_vers);
    mods.sort();
    mods.unique();
    if( ! mods.empty() ){
        params.SetParam("CATEGORY","System & Uncategorized Modules");

        params.StartCycle("MODULES");
        for(CSmallString mod : mods){
            params.SetParam("MODULE",mod);
            params.SetParam("MODULEURL",CFCGIParams::EncodeString(mod));
            params.NextRun();
        }

        params.EndCycle("MODULES");
        params.NextRun();
    }

    params.EndCycle("CATEGORIES");

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
