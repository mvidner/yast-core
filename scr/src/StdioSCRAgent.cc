

/*
 *  StdioSCRAgent.cc
 *
 *  Basic SCR agent wrapper to push evaluation of builtins to another component using standard YCP
 *
 *  Authors:	Stanislav Visnovsky <visnov@suse.cz>
 *  Maintainer:	Arvin Schnell <arvin@suse.de>
 *
 *  $Id$
 */


#include <ycp/y2log.h>
#include <y2/Y2Component.h>
#include "StdioSCRAgent.h"

YCPValue
StdioSCRAgent::Read (const YCPPath &path, const YCPValue &arg, const YCPValue &opt)
{
    if (! m_handler)
	return YCPNull ();
	
    y2debug( "This is StdioSCRAgent(%p)::Read", this );
    
    YCPTerm r ( "Read" );
    r.add (path);
    if (!arg.isNull ()) 
    {
	r.add (arg);
	
	if (! opt.isNull ())
	{
	    r.add (opt);
	}
    }
    
    return m_handler->evaluate (r);
}


YCPBoolean
StdioSCRAgent::Write (const YCPPath &path, const YCPValue &value,
		       const YCPValue &arg)
{
    if (! m_handler)
	return YCPNull ();
	
    y2debug( "This is StdioSCRAgent(%p)::Write", this );
    
    YCPTerm r ( "Write" );
    r.add (path);
    
    if (!value.isNull ()) 
    {
	r.add (value);
	
	if (! arg.isNull ())
	{
	    r.add (arg);
	}
    }
    else
    {
	r.add (YCPVoid ());
    }
    
    YCPValue v = m_handler->evaluate (r);
    return v.isNull () ? YCPNull () : v->asBoolean ();
}


YCPList
StdioSCRAgent::Dir (const YCPPath &path)
{
    if (! m_handler)
	return YCPNull ();
	
    y2debug( "This is StdioSCRAgent(%p)::Dir", this );
    
    YCPTerm r ( "Dir" );
    r.add (path);
    
    YCPValue v = m_handler->evaluate (r);
    return v.isNull () ? YCPNull () : v->asList ();
}


YCPValue
StdioSCRAgent::Execute (const YCPPath &path, const YCPValue &value,
			 const YCPValue &arg)
{
    if (! m_handler)
	return YCPNull ();
	
    y2debug( "This is StdioSCRAgent(%p)::Execute", this );
    
    YCPTerm r ( "Execute" );
    r.add (path);
    if (!value.isNull ()) 
    {
	r.add (value);
	
	if (! arg.isNull ())
	{
	    r.add (arg);
	}
    }
    
    return m_handler->evaluate (r);
}


YCPValue
StdioSCRAgent::otherCommand (const YCPTerm &term)
{
    if (! m_handler)
	return YCPNull ();
	
    y2error( "This is StdioSCRAgent(%p)::otherCommand (unhandled)", this );
    
    return YCPNull ();
}
