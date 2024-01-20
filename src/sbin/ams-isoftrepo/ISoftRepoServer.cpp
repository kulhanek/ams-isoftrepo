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
#include <FCGIRequest.hpp>
#include <ErrorSystem.hpp>
#include <SmallTimeAndDate.hpp>
#include <signal.h>
#include <XMLElement.hpp>
#include <XMLParser.hpp>
#include <TemplatePreprocessor.hpp>
#include <TemplateCache.hpp>
#include <Template.hpp>
#include <XMLPrinter.hpp>
#include <XMLText.hpp>
#include <boost/algorithm/string/replace.hpp>

using namespace std;

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================

CISoftRepoServer ISoftRepoServer;

MAIN_ENTRY_OBJECT(ISoftRepoServer)

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================

CISoftRepoServer::CISoftRepoServer(void)
{
}

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================

int CISoftRepoServer::Init(int argc,char* argv[])
{
    // encode program options, all check procedures are done inside of CABFIntOpts
    int result = Options.ParseCmdLine(argc,argv);

    // should we exit or was it error?
    if( result != SO_CONTINUE ) return(result);

    // attach verbose stream to terminal stream and set desired verbosity level
    vout.Attach(Console);
    if( Options.GetOptVerbose() ) {
        vout.Verbosity(CVerboseStr::high);
    } else {
        vout.Verbosity(CVerboseStr::low);
    }

    CSmallTimeAndDate dt;
    dt.GetActualTimeAndDate();

    vout << low;
    vout << endl;
    vout << "# ==============================================================================" << endl;
    vout << "# isoftrepo.fcgi (AMS utility) started at " << dt.GetSDateAndTime() << endl;
    vout << "# ==============================================================================" << endl;

    // load server config
    if( LoadConfig() == false ) return(SO_USER_ERROR);

    return(SO_CONTINUE);
}

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================

bool CISoftRepoServer::Run(void)
{
    // CtrlC signal
    signal(SIGINT,CtrlCSignalHandler);
    signal(SIGTERM,CtrlCSignalHandler);

    SetPort(GetPortNumber());

    // start servers
    Watcher.StartThread(); // watcher
    if( StartServer() == false ) { // and fcgi server
        return(false);
    }

    vout << low;
    vout << "Waiting for server termination ..." << endl;
    WaitForServer();

    Watcher.TerminateThread();
    Watcher.WaitForThread();

    return(true);
}

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================

void CISoftRepoServer::Finalize(void)
{
    CSmallTimeAndDate dt;
    dt.GetActualTimeAndDate();

    vout << low;
    vout << "# ==============================================================================" << endl;
    vout << "# isoftrepo.fcgi (AMS utility) terminated at " << dt.GetSDateAndTime() << endl;
    vout << "# ==============================================================================" << endl;

    if( ErrorSystem.IsError() || Options.GetOptVerbose() ){
        vout << low;
        ErrorSystem.PrintErrors(vout);
    }

    vout << endl;
}

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================

bool CISoftRepoServer::AcceptRequest(void)
{
    // this is simple FCGI Application with 'Hello world!'

    CFCGIRequest request;

    // accept request
    if( request.AcceptRequest(this) == false ) {
        ES_ERROR("unable to accept request");
        // unable to accept request
        return(false);
    }

    request.Params.LoadParamsFromQuery();

    // write document
    request.OutStream.PutStr("Content-type: text/html\r\n");
    request.OutStream.PutStr("\r\n");

    // get request id
    CSmallString action;
    action = request.Params.GetValue("action");

    bool result = false;

    // list categories -----------------------------
    if( (action == NULL) || (action == "categories") ) {
        result = _ListCategories(request);
    }

    // module info -----------------------------
    if( action == "module" ) {
        result = _Module(request);
    }

    // versions info -----------------------------
    if( action == "version" ) {
        result = _Version(request);
    }

    // build -----------------------------
    if( action == "build" ) {
        result = _Build(request);
    }

    // error handle -----------------------
    if( result == false ) {
        ES_ERROR("error");
        result = _Error(request);
    }
    if( result == false ) request.FinishRequest(); // at least try to finish request

    return(true);
}

//------------------------------------------------------------------------------

bool CISoftRepoServer::ProcessTemplate(CFCGIRequest& request,
                                       const CSmallString& template_name,
                                       CTemplateParams& template_params)
{
    // template --------------------------------------------------------
    CTemplate* p_tmp = TemplateCache.OpenTemplate(template_name);

    if( p_tmp == NULL ) {
        ES_ERROR("unable to open template");
        return(false);
    }

    // preprocess template ---------------------------------------------
    CTemplatePreprocessor preprocessor;
    CXMLDocument          output_xml;

    preprocessor.SetInputTemplate(p_tmp);
    preprocessor.SetOutputDocument(&output_xml);

    if( preprocessor.PreprocessTemplate(&template_params) == false ) {
        ES_ERROR("unable to preprocess template");
        return(false);
    }

    // print output ----------------------------------------------------
    CXMLPrinter xml_printer;

    xml_printer.SetPrintedXMLNode(&output_xml);
    xml_printer.SetPrintAsItIs(true);

    unsigned char* p_data;
    unsigned int   len = 0;

    if( (p_data = xml_printer.Print(len)) == NULL ) {
        ES_ERROR("unable to print output");
        return(false);
    }

    request.OutStream.PutStr((const char*)p_data,len);

    delete[] p_data;

    request.FinishRequest();

    return(true);
}

//------------------------------------------------------------------------------

bool CISoftRepoServer::ProcessCommonParams(CFCGIRequest& request,
        CTemplateParams& template_params)
{
    CSmallString server_script_uri;

    if( request.Params.GetValue("SERVER_PORT") == "443" ) {
        server_script_uri = "https://";
    } else {
        server_script_uri = "http://";
    }

    server_script_uri << request.Params.GetValue("SERVER_NAME");
    if( request.Params.GetValue("SCRIPT_NAME").GetLength() >= 1 ){
        if( request.Params.GetValue("SCRIPT_NAME")[0] != '/' ){
            server_script_uri << "/";
        }
    }
    server_script_uri << request.Params.GetValue("SCRIPT_NAME");

    // FCGI setup
    template_params.SetParam("SERVERSCRIPTURI",server_script_uri);

    return(true);
}

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================

void CISoftRepoServer::CtrlCSignalHandler(int signal)
{
    ISoftRepoServer.vout << endl << endl;
    ISoftRepoServer.vout << "SIGINT or SIGTERM signal recieved. Initiating server shutdown!" << endl;
    ISoftRepoServer.vout << "Waiting for server finalization ... " << endl;
    ISoftRepoServer.TerminateServer();
    if( ! ISoftRepoServer.Options.GetOptVerbose() ) ISoftRepoServer.vout << endl;
}

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================

bool CISoftRepoServer::LoadConfig(void)
{
    CFileName config_path =  Options.GetArgConfigFile();

    CXMLParser xml_parser;
    xml_parser.SetOutputXMLNode(&ServerConfig);
    xml_parser.EnableWhiteCharacters(true);

    if( xml_parser.Parse(config_path) == false ) {
        CSmallString error;
        error << "unable to load server config";
        ES_ERROR(error);
        return(false);
    }

    CFileName temp_dir = GetTemplatePath();
    TemplateCache.SetTemplatePath(temp_dir);

    vout << "#" << endl;
    vout << "# === [server] =================================================================" << endl;
    vout << "# FCGI Port  = " << GetPortNumber() << endl;
    vout << "# Templates  = " << temp_dir << endl;
    vout << "#" << endl;

    BundleName = GetBundleName();
    BundlePath = GetBundlePath();

    vout << "#" << endl;
    vout << "# === [ams-bundles] ============================================================" << endl;
    vout << "# Name      = " << BundleName << endl;
    vout << "# Path      = " << BundlePath << endl;
    vout << "#" << endl;

    CXMLElement* p_watcher = ServerConfig.GetChildElementByPath("config/watcher");
    Watcher.ProcessWatcherControl(vout,p_watcher);
    vout << "#" << endl;

    return(true);
}

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================

int CISoftRepoServer::GetPortNumber(void)
{
    int setup = 32696;
    CXMLElement* p_ele = ServerConfig.GetChildElementByPath("config/server");
    if( p_ele == NULL ) {
        ES_ERROR("unable to open config path");
        return(setup);
    }
    if( p_ele->GetAttribute("port",setup) == false ) {
        ES_ERROR("unable to get port value");
        return(setup);
    }
    return(setup);
}

//------------------------------------------------------------------------------

const CFileName CISoftRepoServer::GetTemplatePath(void)
{
    CFileName temp_dir = "/opt/ams-isoftrepo/9.0/var/html/isoftrepo/template";

    CXMLElement* p_ele = ServerConfig.GetChildElementByPath("config/server");
    if( p_ele == NULL ) {
        ES_ERROR("unable to open config path");
        return(temp_dir);
    }
    if( p_ele->GetAttribute("templates",temp_dir) == false ) {
        ES_ERROR("unable to get templates values");
        return(temp_dir);
    }
    return(temp_dir);
}

//------------------------------------------------------------------------------

const CSmallString CISoftRepoServer::GetBundleName(void)
{
    CSmallString name;
    CXMLElement* p_ele = ServerConfig.GetChildElementByPath("config/ams");
    if( p_ele == NULL ) {
        ES_ERROR("unable to open config/ams path");
        return(name);
    }
    if( p_ele->GetAttribute("name",name) == false ) {
        ES_ERROR("unable to get name item");
        return(name);
    }
    return(name);
}

//------------------------------------------------------------------------------

const CFileName CISoftRepoServer::GetBundlePath(void)
{
    CSmallString path;
    CXMLElement* p_ele = ServerConfig.GetChildElementByPath("config/ams");
    if( p_ele == NULL ) {
        ES_ERROR("unable to open config/ams path");
        return(path);
    }
    if( p_ele->GetAttribute("path",path) == false ) {
        ES_ERROR("unable to get path item");
        return(path);
    }
    return(path);
}

//------------------------------------------------------------------------------

CXMLElement* CISoftRepoServer::GetMonitoringIFrame(void)
{
    CXMLElement* p_ele = ServerConfig.GetChildElementByPath("config/monitoring",true);
    return(p_ele);
}

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================
