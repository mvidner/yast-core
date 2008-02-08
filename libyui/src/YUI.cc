/*---------------------------------------------------------------------\
|								       |
|		       __   __	  ____ _____ ____		       |
|		       \ \ / /_ _/ ___|_   _|___ \		       |
|			\ V / _` \___ \ | |   __) |		       |
|			 | | (_| |___) || |  / __/		       |
|			 |_|\__,_|____/ |_| |_____|		       |
|								       |
|				core system			       |
|							 (C) SuSE GmbH |
\----------------------------------------------------------------------/

  File:		YUI.cc

  Authors:	Stefan Hundhammer <sh@suse.de>
		Stanislav Visnovsky <visnov@suse.cz>

  Maintainer:	Stefan Hundhammer <sh@suse.de>

/-*/


#define VERBOSE_COMM 0	// VERY verbose thread communication logging

#include <stdio.h>
#include <string.h>	// strerror()
#include <unistd.h>	// pipe()
#include <fcntl.h>  	// fcntl()
#include <errno.h>
#include <pthread.h>

#define YUILogComponent "ui"
#include "YUILog.h"

#include "Y2UINamespace.h"
#include "YUI.h"
#include "YUISymbols.h"
#include "YDialog.h"
#include "YApplication.h"
#include "YMacro.h"
#include "YBuiltinCaller.h"



YUI * YUI::_yui = 0;

extern void *start_ui_thread( void * yui );


YUI::YUI( bool withThreads )
    : _withThreads( withThreads )
    , _uiThread( 0 )
    , _builtinCaller( 0 )
    , _terminate_ui_thread( false )
    , _eventsBlocked( false )
{
    _yui = this;
}


YUI::~YUI()
{
    if ( _withThreads && _uiThread )
    {
	yuiError() << "shutdownThreads() was never called!"     << endl;
	yuiError() << "shutting down now - this might segfault" << endl;
	shutdownThreads();
    }

    if ( YDialog::openDialogsCount() > 0 )
	yuiError() << YDialog::openDialogsCount() << " open dialogs left over" << endl;

    if ( _builtinCaller )
	delete _builtinCaller;

    YDialog::deleteAllDialogs();

    YMacro::deleteRecorder();
    YMacro::deletePlayer();
}


YUI *
YUI::ui()
{
    YUI_CHECK_PTR( _yui );
    return _yui;
}


YWidgetFactory *
YUI::widgetFactory()
{
    static YWidgetFactory * factory = 0;

    if ( ! factory )
	factory = ui()->createWidgetFactory();

    YUI_CHECK_PTR( factory );
    return factory;
}


YOptionalWidgetFactory *
YUI::optionalWidgetFactory()
{
    static YOptionalWidgetFactory * factory = 0;

    if ( ! factory )
	factory = ui()->createOptionalWidgetFactory();

    YUI_CHECK_PTR( factory );
    return factory;
}


YApplication *
YUI::app()
{
    static YApplication * app = 0;

    if ( ! app )
	app = ui()->createApplication();

    YUI_CHECK_PTR( app );
    return app;
}


void YUI::topmostConstructorHasFinished()
{
    // The ui thread must not be started before the constructor
    // of the actual user interface is finished. Otherwise there
    // is a race condition. The ui thread would go into idleLoop()
    // before the ui is really setup. For example the Qt interface
    // does a processOneEvent in the idleLoop(). This may do a
    // repaint operation on the dialog that is just under construction!
    //
    // Therefore the creation of the thread is delayed and performed in this
    // method. It must be called at the end of the constructor of the specific
    // UI (the Qt UI or the NCurses UI).

    if ( _withThreads )
    {
	if ( pipe( pipe_from_ui ) == 0 &&
	     pipe( pipe_to_ui   ) == 0   )
	{

	    // Make fd non blockable the ui thread reads from
	    long arg;
	    arg = fcntl( pipe_to_ui[0], F_GETFL );
	    if ( fcntl( pipe_to_ui[0], F_SETFL, arg | O_NONBLOCK ) < 0 )
	    {
		yuiError() << "Couldn't set O_NONBLOCK: errno: " << errno << " " << strerror( errno ) << endl;
		_withThreads = false;
		close( pipe_to_ui[0] );
		close( pipe_to_ui[1] );
		close( pipe_from_ui[0] );
		close( pipe_from_ui[1] );
	    }
	    else
	    {
		_terminate_ui_thread = false;
		createUIThread();
	    }
	}
	else
	{
	    yuiError() << "pipe() failed: errno: " << errno << " " << strerror( errno ) << endl;
	    exit(2);
	}
    }
    else
    {
	yuiMilestone() << "Running without threads" << endl;
    }
}


void YUI::createUIThread()
{
    pthread_attr_t attr;
    pthread_attr_init( & attr );
    pthread_create( & _uiThread, & attr, start_ui_thread, this );
}


void YUI::terminateUIThread()
{
    _terminate_ui_thread = true;
    signalUIThread();
    waitForUIThread();
    pthread_join( _uiThread, 0 );
}


void YUI::shutdownThreads()
{
    if ( _uiThread )
    {
	terminateUIThread();
	_uiThread = 0;
	close( pipe_to_ui[0] );
	close( pipe_to_ui[1] );
	close( pipe_from_ui[0] );
	close( pipe_from_ui[1] );
    }
}


void YUI::signalUIThread()
{
    static char arbitrary = 42;
    (void) write ( pipe_to_ui[1], & arbitrary, 1 );
}


bool YUI::waitForUIThread()
{
    char arbitrary;
    int res;

    do {
	res = read( pipe_from_ui[0], & arbitrary, 1 );
	if ( res == -1 )
	{
	    if ( errno == EINTR || errno == EAGAIN )
		continue;
	    else
		yuiError() <<  "waitForUIThread: errno: " << errno << " " << strerror( errno ) << endl;
	}
    } while ( res == 0 );

    // return true if we really did get a signal byte
    return res != -1;
}


void YUI::signalYCPThread()
{
    static char arbitrary;
    (void) write( pipe_from_ui[1], & arbitrary, 1 );

#if VERBOSE_COMM
    yuiDebug() << "Wrote byte to YCP thread: " << endl;
#endif
}


bool YUI::waitForYCPThread()
{
    char arbitrary;

    int res;
    do {
#if VERBOSE_COMM
	yuiDebug() << "Waiting for YCP thread..." << endl;
#endif
	res = read( pipe_to_ui[0], & arbitrary, 1 );
	if ( res == -1 )
	{
	    if ( errno == EINTR || errno == EAGAIN )
		continue;
	    else
		yuiError() << "waitForYCPThread: errno: " << errno << " " << strerror( errno ) << endl;
	}
    } while ( res == 0 );

#if VERBOSE_COMM
    yuiDebug() << "Read byte from YCP thread" << endl;
#endif

    // return true if we really did get a signal byte
    return res != -1;
}


void YUI::uiThreadMainLoop()
{
    while ( true )
    {
	idleLoop ( pipe_to_ui[0] );

	// The pipe is non-blocking, so we have to check if we really read a
	// signal byte. Although idleLoop already does a select(), this seems to
	// be necessary.  Anyway: Why do we set the pipe to non-blocking if we
	// wait in idleLoop for it to become readable? It is needed in
	// YUIQt::idleLoop for QSocketNotifier.

	if ( ! waitForYCPThread () )
	    continue;

	if ( _terminate_ui_thread )
	{
	    signalYCPThread();
	    yuiDebug() << "Shutting down UI main loop" << endl;
	    return;
	}

	if ( _builtinCaller )
	    _builtinCaller->call();
	else
	    yuiError() << "No builtinCaller set" << endl;

	signalYCPThread();
    }
}


// ----------------------------------------------------------------------


void *start_ui_thread( void * yui )
{
    YUI * ui= (YUI *) yui;

#if VERBOSE_COMM
    yuiDebug() << "Starting UI thread" << endl;
#endif

    if ( ui )
	ui->uiThreadMainLoop();
    return 0;
}




// FIXME: Move this to another class
// (YUI should become independent of YCP and the YaST2 infrastructure)

bool YUI::debugLoggingEnabled() const
{
    return get_log_debug();
}


void YUI::enableDebugLogging( bool enable )
{
    YUILog::enableDebugLogging( enable );
    set_log_debug( enable );
}



// EOF