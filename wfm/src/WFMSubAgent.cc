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

   File:	WFMSubAgent.cc

   Author:	Arvin Schnell <arvin@suse.de>
   Maintainer:	Arvin Schnell <arvin@suse.de>

/-*/


#include <ycp/y2log.h>
#include <y2/Y2ComponentBroker.h>
#include <scr/SCRAgent.h>
#include <WFMSubAgent.h>


WFMSubAgent::WFMSubAgent (const string& name, int handle)
    : my_name (name),
      my_handle (handle),
      my_comp (0),
      my_agent (0)
{
    // check if name is scr ( can be prepended by chroot like "chroot=/mnt:scr" )
    if (name.find("scr") == string::npos && name.find("chroot") != string::npos)
    {
      y2internal("WFMSubAgent supports chroot only for scr component, but not '%s'.",
        name.c_str());
      abort();
    }
}


WFMSubAgent::~WFMSubAgent ()
{
}


bool
WFMSubAgent::start ()
{
    if (!my_comp)
    {
	y2debug ("Creating SubAgent: %d %s", my_handle, my_name.c_str ());
        my_comp = Y2ComponentBroker::createServer (my_name.c_str ());
    }

    return my_comp != 0;
}
