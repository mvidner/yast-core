/*
 * YaST2: Core system
 *
 * Description:
 *   YaST2 logging implementation
 *
 * Authors:
 *   Mathias Kettner <kettner@suse.de>
 *   Thomas Roelz <tom@suse.de>
 *   Michal Svec <msvec@suse.cz>
 *
 * $Id$
 */


#define _GNU_SOURCE 1		// Needed for vasprintf below

#include <errno.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <malloc.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "YCPParser.h"
#include "YCPValue.h"
#include "toString.h"

#include "y2log.h"

/* Defines */

#define Y2LOG_MAXSIZE	1024 * 1024		/* Maximal logfile size */
#define Y2LOG_MAXNUM	10			/* Maximum logfiles number */

static FILE * Y2LOG_STDERR = stderr;            /* Default output */
#define Y2LOG_DATE	"%Y-%m-%d %H:%M:%S"	/* The date format */

#define Y2LOG_FORMAT	"%s <%d> %s(%d)%s %s%s:%d %s%s"

#define LOGDIR		"/var/log/YaST2"
#define Y2LOG_ROOT	LOGDIR "/y2log"
#define Y2LOG_USER	"/.y2log"		/* Relative to $HOME */
#define Y2LOG_CONF	"/.yast2/logconf.ycp"	/* Relative to $HOME */
#define Y2LOG_FALLBACK	"/y2log"

#define Y2LOG_VAR_DEBUG	"Y2DEBUG"
#define Y2LOG_VAR_ALL	"Y2DEBUGALL"
#define Y2LOG_VAR_SIZE	"Y2MAXLOGSIZE"
#define Y2LOG_VAR_NUM	"Y2MAXLOGNUM"


// Tue Jan 29 16:51:39 CET 2002 - ma@suse.de
// y2log must use a private copy of stderr, esp. in case we're always logging to
// it (option "-l -"). Some classes like liby2(ExternalProgram) redirect stderr in
// order to redirect an external programs error output. As a side effect y2log output
// done after the redirection would show up in the external programs output file instead
// of yast2's stderr.
static int dupStderr() {
  int dupstderr = dup( 2 );
  if ( dupstderr != -1 ) {
    FILE * newstderr = fdopen( dupstderr, "a" );
    if ( newstderr == NULL ) {
      fprintf( Y2LOG_STDERR, "y2log: Can't fdopen new stderr: %s.\n", strerror (errno) );
    } else {
      fcntl (fileno (newstderr), F_SETFD, fcntl (fileno (newstderr), F_GETFD) | FD_CLOEXEC);
      Y2LOG_STDERR = newstderr;
    }
  } else {
    fprintf( Y2LOG_STDERR, "y2log: Can't dup stderr: %s.\n", strerror (errno) );
  }
  return 1;
}
static int x = dupStderr();
// Tue Jan 29 16:51:39 CET 2002 - ma@suse.de


/* Global prototypes */

static void	shiftLogfiles( string filename );

/* Global variables */

static string	y2logfilename;
static int	y2maxlogsize;
static int	y2maxlognum;
static int	y2log_initialized;
static YCPValue	y2logconf = YCPVoid();

/* The universal logger function */

void
y2_logger(loglevel_t level, const char *component, const char *file,
	  const int line, const char *func, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    y2_vlogger(level, component, file, line, func, format, ap);
    va_end(ap);
}

/* The universal logger function */

void
y2_vlogger(loglevel_t level, const char *component, const char *file,
	   const int line, const char *function, const char *format, va_list ap)
{
    /* Prepare the logfile name */
    if (!y2log_initialized)
	y2setLogfileName (""); /* The default */

    /* Prepare the log text */
    char *logtext = NULL;
    vasprintf(&logtext, format, ap); /* GNU extension needs the define above */

    /* Prepare the PID */
    pid_t pid = getpid();

    /* Prepare the host name */
    char hostname[1024];
    if (gethostname(hostname, 1024))
	strncpy(hostname, "unknown", 1024);

    /* Prepare the logfile */
    shiftLogfiles (y2logfilename);

    FILE *logfile = Y2LOG_STDERR;
    if (y2logfilename != "-") {
	logfile = fopen (y2logfilename.c_str (), "a");
	if (!logfile) {
	    fprintf (Y2LOG_STDERR, "y2log: Error opening logfile '%s': %s.\n",
		     y2logfilename.c_str (), strerror (errno));
	    return;
	}
    }

    /* Prepare the date */
#if 1
    // just 1 second precision
    time_t timestamp = time (NULL);
    struct tm *brokentime = localtime (&timestamp);
    char date[50];		// that's big enough
    strftime (date, sizeof (date), Y2LOG_DATE, brokentime);
#else
    // 1 millisecond precision (use only for testing)
    timeval time;
    gettimeofday (&time, NULL);
    time_t timestamp = time.tv_sec;
    struct tm *brokentime = localtime (&timestamp);
    char tmp1[50], date[50];	// that's big enough
    strftime (tmp1, sizeof (tmp1), Y2LOG_DATE, brokentime);
    snprintf (date, sizeof (date), "%s.%03ld", tmp1, time.tv_usec / 1000);
#endif

#if 0
    // print "bytes from system" and "bytes in use" per thread (use only
    // for testing)
    struct mallinfo mi;
    mi = mallinfo ();
    char tmp2[50];
    snprintf (tmp2, sizeof (tmp2), " [%5dkB %5dkB]", mi.arena >> 10,
	      mi.uordblks >> 10);
    strcat (date, tmp2);
#endif

    /* Prepare the component */
    string comp = component;
    if (!comp.empty ())
	comp = " [" + comp + "]";

    /* Prepare the component */
    string func = function;
    if (!func.empty ())
	func = "(" + func + ")";

    /* Prepare the file, strip rooted path  */

    if (*file == '/')		     // rooted path
    {
	char *slashptr = strrchr (file, '/');
	if (slashptr > file)		    // last slash is second slash
	{
	    char *slashptr2 = slashptr-1;

	    // find last but one slash

	    while ((slashptr2 > file) && (*slashptr2 != '/'))
	    {
		slashptr2--;
	    }
	    // if last but one slash exists, use this as start
	    if (slashptr2 != file)
		file = slashptr2 + 1;
	}
    }

    /* do we need EOL? */
    bool eol = false;
    size_t len = strlen(logtext);
    if ((len==0) || ((len>0) && (logtext[len-1]!='\n')))
	eol = true;

    /* Do the log */
    fprintf (logfile, Y2LOG_FORMAT, date, level, hostname, pid, comp.c_str (),
	    file, func.c_str (), line, logtext, eol?"\n":"");

    /* Clean everything */
    if (logfile && logfile != Y2LOG_STDERR)
	fclose (logfile);
    else
	fflush (logfile);

    if (logtext)
	free (logtext);
}

/* Logfile name initialization */

void
y2setLogfileName (string filename)
{
    struct passwd *pw = getpwuid( geteuid() );

    char *env_maxlogsize = getenv("Y2MAXLOGSIZE");
    y2maxlogsize = env_maxlogsize ? atoi(env_maxlogsize) * 1024 : Y2LOG_MAXSIZE;

    char *env_maxlognum = getenv("Y2MAXLOGNUM");
    y2maxlognum = env_maxlognum ? atoi(env_maxlognum) : Y2LOG_MAXNUM;

    /* Assign logfile name */

    if (filename.empty ()) {	    /* No filename --> use defaults */
	if (geteuid()) {	/* Non root */
	    if (!pw) {
		fprintf( Y2LOG_STDERR,
			 "Cannot read pwd entry of user id %d. Logfile will be '%s'.\n",
			 geteuid(), Y2LOG_FALLBACK );

		y2logfilename = Y2LOG_FALLBACK;
	    }
	    else
		y2logfilename = string(pw->pw_dir) + Y2LOG_USER;
	}
	else		    /* Root */
	    y2logfilename = Y2LOG_ROOT;
    }
    else
	y2logfilename = filename;  /* Explicit assignment */

    /* Read the logconf.ycp */

    if (pw)
    {
	string logconfname = string (pw->pw_dir) + Y2LOG_CONF;

	// We have to remember the errno. Otherwise a call of
	// y2error ("error: %m") can display a wrong message.
	int save_errno = errno;

	FILE *file = fopen (logconfname.c_str (), "r");
	if (file && (y2log_initialized < 1))
	{
	    y2log_initialized = 1;
	    YCPParser parser (file, logconfname.c_str ());
	    y2logconf = parser.parse ();
	    fclose (file);
	}

	errno = save_errno;

    }
    else
    {
	fprintf (Y2LOG_STDERR, "Cannot read pwd entry for user id %d. No "
		 "logconf, using defaults.\n", geteuid ());
    }

    y2log_initialized = 2;
}


/* Logfile name reporting */

string
y2getLogfileName ()
{
    if (!y2log_initialized)
	y2setLogfileName (""); /* The default */

    return y2logfilename;
}


/* Maintain logfiles */

static void
shiftLogfiles( string filename )
{
    struct stat buf;

    if( stat(filename.c_str(), &buf) )
	return;

    if( buf.st_size <= y2maxlogsize )
	return;

    /* Delete the last logfile, rename existing ones */
    remove( (filename + "-" + ::toString(y2maxlognum - 1)).c_str() );

    for( int f = y2maxlognum-2; f > 0; f-- )
	rename( (filename + "-" + ::toString(f)).c_str(),
		(filename + "-" + ::toString(f+1)).c_str() );

    rename( filename.c_str(), (filename + "-1").c_str() );
}

/* Check if logging is advised (logconf.ycp) */

bool
shouldBeLogged (int loglevel, string componentname)
{
    /* Everything should be logged */
    if (getenv(Y2LOG_VAR_ALL))
	return true;

    /* Prepare the logfile name */
    if (!y2log_initialized)
	y2setLogfileName (""); /* The default */

    if( !y2logconf.isNull() ) {

	/* Logconf is a list of entries. One entry is for one component */
	if( y2logconf->isList() ) {
	    YCPList l = y2logconf->asList();

	    for ( int i = 0; i < l->size(); i++ ) {

		/* Entry is a tuple (list) of component name and list of enablings */
		if( l->value(i)->isList() ) {
		    YCPList entry = l->value(i)->asList();

		    /* Check if it is valid. */
		    if( entry->value(0)->isString() && entry->value(1)->isList() )
		    {
			if( componentname == entry->value(0)->asString()->value() )
			{
			    YCPList enabling = entry->value(1)->asList();
			    if( loglevel < enabling->size() )
			    {
				if( enabling->value(loglevel)->isBoolean() )
				    return enabling->value(loglevel)->asBoolean()->value();
				else
				    return true; /* Default is to turn on. */
			    }
			    else
				return true; /* Default is to turn on. */
			}
		    }
		}
	    }
	}
    }

    return ( loglevel != LOG_DEBUG || getenv(Y2LOG_VAR_DEBUG) );
}

/* Testing main() */

#ifdef _MAIN
int
main()
{
    y2setLogfileName("-");
    y2debug("D: %s","1");
    y2error("E: %d\n",3);
    y2error("\n");
    y2error("");
    y2debug("D: %s","2");
    ycp2debug("File.ycp",13,"Debug: %d",7);
    ycp2error("File.ycp",14,"Error: %d",8);
    syn2error("File.ycp",14,"Parser error: %d",8);
    return 0;
}
#endif

/* EOF */
