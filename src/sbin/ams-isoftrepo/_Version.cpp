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

using namespace std;

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================

bool CISoftRepoServer::_Version(CFCGIRequest& request)
{
    // parameters ------------------------------------------------------
    CTemplateParams    params;

    params.Initialize();
    params.SetParam("AMSVER",LibBuildVersion_AMS_Web);
    params.Include("MONITORING",GetMonitoringIFrame());

    ProcessCommonParams(request,params);

    // IDs ------------------------------------------
    CSmallString module_name,module_ver,modver;

    CModUtils::ParseModuleName(request.Params.GetValue("module"),module_name,module_ver);
    modver = module_name + ":" + module_ver;

    params.SetParam("MODVER",modver);
    params.SetParam("MODVERURL",CFCGIParams::EncodeString(modver));
    params.SetParam("MODULE",module_name);
    params.SetParam("MODULEURL",CFCGIParams::EncodeString(module_name));
    params.SetParam("VERSION",module_ver);

    // populate cache ------------
    CModuleController mod_controller;
    mod_controller.InitModuleControllerConfig(BundleName,BundlePath);
    mod_controller.LoadBundles(EMBC_SMALL);
    CModCache mod_cache;
    mod_controller.MergeBundles(mod_cache);

    // get module
    CXMLElement* p_module = mod_cache.GetModule(module_name);
    if( p_module == NULL ) {
        ES_ERROR("module record was not found");
        return(false);
    }

    // list of builds ----------------------------
    std::list<CSmallString>   builds;
    CModCache::GetModuleBuildsSorted(p_module,module_ver,builds);

    params.StartCycle("BUILDS");
    for(CSmallString bld_name: builds){
        CSmallString full_name;
        full_name << module_name << ":" << bld_name;
        params.SetParam("BUILD",full_name);
        params.SetParam("TBUILD",CFCGIParams::EncodeString(full_name));
        params.NextRun();
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
