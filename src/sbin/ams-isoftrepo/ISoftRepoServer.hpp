#ifndef ISoftRepoServerH
#define ISoftRepoServerH
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

#include <FCGIServer.hpp>
#include <XMLDocument.hpp>
#include <FileName.hpp>
#include <TemplateParams.hpp>
#include "ISoftRepoOptions.hpp"
#include <VerboseStr.hpp>
#include <TerminalStr.hpp>
#include <ServerWatcher.hpp>

//------------------------------------------------------------------------------

class CISoftRepoServer : public CFCGIServer {
public:
    CISoftRepoServer(void);

// main methods ----------------------------------------------------------------
    /// init options
    int Init(int argc,char* argv[]);

    /// main part of program
    bool Run(void);

    /// finalize
    void Finalize(void);

// section of private data -----------------------------------------------------
private:
    CISoftRepoOptions   Options;
    CXMLDocument        ServerConfig;
    CTerminalStr        Console;
    CVerboseStr         vout;
    CServerWatcher      Watcher;

    static  void CtrlCSignalHandler(int signal);

    virtual bool AcceptRequest(void);

    // web pages handlers ------------------------------------------------------
    bool _ListSites(CFCGIRequest& request);
    bool _SiteInfo(CFCGIRequest& request);
    bool _ListCategories(CFCGIRequest& request);
    bool _Module(CFCGIRequest& request);
    bool _Version(CFCGIRequest& request);
    bool _Build(CFCGIRequest& request);
    bool _Search(CFCGIRequest& request);
    bool _SearchSites(CFCGIRequest& request);
    bool _SearchSite(CFCGIRequest& request);
    bool _Error(CFCGIRequest& request);

    bool ProcessCommonParams(CFCGIRequest& request,
                             CTemplateParams& template_params);

    bool ProcessTemplate(CFCGIRequest& request,
                         const CSmallString& template_name,
                         CTemplateParams& template_params);

    // configuration options ---------------------------------------------------
    bool LoadConfig(void);

    // fcgi server
    int                GetPortNumber(void);

    // ams root
    const CSmallString GetAMSRoot(void);

    // description
    const CSmallString GetLocationName(void);
    const CSmallString GetHomeURL(void);
    const CSmallString GetHomeText(void);

    // monitoring
    CXMLElement* GetMonitoringIFrame(void);
};

//------------------------------------------------------------------------------

extern CISoftRepoServer ISoftRepoServer;

//------------------------------------------------------------------------------

#endif
