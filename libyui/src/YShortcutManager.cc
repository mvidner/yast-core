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

  File:		YShortcutManager.cc

  Author:	Stefan Hundhammer <sh@suse.de>

/-*/


#define y2log_component "ui-shortcuts"
#include <ycp/y2log.h>
#include "YShortcutManager.h"
#include "YDialog.h"


// Return the number of elements of an array of any type
#define DIM( ARRAY )	( (int) ( sizeof( ARRAY)/( sizeof( ARRAY[0] ) ) ) )


YShortcutManager::YShortcutManager( YDialog *dialog )
    : _dialog( dialog )
    , _conflictCount( 0 )
    , _didCheck( false )
{
    YUI_CHECK_PTR( _dialog );
}


YShortcutManager::~YShortcutManager()
{
    clearShortcutList();
}


void
YShortcutManager::checkShortcuts( bool autoResolve )
{
    y2debug( "Checking keyboard shortcuts" );

    clearShortcutList();
    findShortcutWidgets( _dialog->childrenBegin(), _dialog->childrenEnd() );


    // Initialize wanted character counters
    for ( int i=0; i < DIM( _wanted ); i++ )
	_wanted[i] = 0;

    // Initialize used character flags
    for ( int i=0; i < DIM( _wanted ); i++ )
	_used[i] = false;

    // Count wanted shortcuts
    for ( unsigned i=0; i < _shortcutList.size(); i++ )
	_wanted[ (int) _shortcutList[i]->preferred() ]++;


    // Report errors

    _conflictCount = 0;

    for ( unsigned i=0; i < _shortcutList.size(); i++ )
    {
	YShortcut *shortcut = _shortcutList[i];

	if ( YShortcut::isValid( shortcut->preferred() ) )
	{
	    if ( _wanted[ (int) shortcut->preferred() ] > 1 )	// shortcut char used more than once
	    {
		shortcut->setConflict();
		_conflictCount++;
		y2milestone( "Shortcut conflict: '%c' used for %s \"%s\"",
			     shortcut->preferred(),
			     shortcut->widgetClass(),
			     shortcut->shortcutString().c_str() );
	    }
	}
	else	// No or invalid shortcut
	{
	    if ( shortcut->cleanShortcutString().length() > 0 )
	    {
		shortcut->setConflict();
		_conflictCount++;

		if ( ! shortcut->widget()->autoShortcut() )
		{
		    y2milestone( "No valid shortcut for %s \"%s\"",
				 shortcut->widgetClass(),
				 shortcut->cleanShortcutString().c_str() );
		}
	    }
	}

	if ( ! shortcut->conflict() )
	{
	    _used[ (int) shortcut->preferred() ] = true;
	}
    }

    _didCheck = true;

    if ( _conflictCount > 0 )
    {
	if ( autoResolve )
	{
	    resolveAllConflicts();
	}
    }
    else
    {
	y2debug( "No shortcut conflicts" );
    }

#if 0
    for ( unsigned i=0; i < _shortcutList.size(); i++ )
    {
	YShortcut *shortcut = _shortcutList[i];

	y2debug( "Shortcut '%c' for %s \"%s\" ( original: \"%s\" )",
		 shortcut->shortcut() ? shortcut->shortcut() : ' ',
		 shortcut->widgetClass(),
		 shortcut->cleanShortcutString().c_str(),
		 shortcut->shortcutString().c_str() );
    }
#endif
}


void
YShortcutManager::resolveAllConflicts()
{
    y2debug( "Resolving shortcut conflicts" );

    if ( ! _didCheck )
    {
	y2milestone( "Call checkShortcuts() first!" );
	return;
    }


    // Make a list of all shortcuts with conflicts

    YShortcutList conflictList;
    _conflictCount = 0;

    for ( YShortcutListIterator it = _shortcutList.begin();
	  it != _shortcutList.end();
	  ++it )
    {
	if ( ( *it )->conflict() )
	{
	    conflictList.push_back( *it );
	    _conflictCount++;
	}
    }


    // Resolve each conflict

    while ( ! conflictList.empty() )
    {
	//
	// Pick a conflict widget to resolve.
	//

	// Wizard buttons have priority - check any of them first.
	int prioIndex = findShortestWizardButton( conflictList );

	if ( prioIndex < 0 )
	    prioIndex = findShortestWidget( conflictList); // Find the shortest widget. Buttons have priority.


	// Pick a new shortcut for this widget.

	YShortcut * shortcut = conflictList[ prioIndex ];
	resolveConflict( shortcut );

	if ( shortcut->conflict() )
	{
	    y2milestone( "Couldn't resolve shortcut conflict for %s \"%s\"",
			 shortcut->widgetClass(), shortcut->cleanShortcutString().c_str() );

	}


	// Mark this particular conflict as resolved.

	conflictList.erase( conflictList.begin() + prioIndex );
    }

    if ( _conflictCount > 0 )
    {
	y2debug( "%d shortcut conflict(s) left", _conflictCount );
    }
}



void
YShortcutManager::resolveConflict( YShortcut * shortcut )
{
    // y2debug( "Picking shortcut for %s \"%s\"", shortcut->widgetClass(), shortcut->cleanShortcutString().c_str() );

    char candidate = shortcut->preferred();			// This is always normalized, no need to normalize again.

    if ( ! YShortcut::isValid( candidate )			// Can't use this character - pick another one.
	 || _used[ (int) candidate ] )
    {
	candidate = 0;						// Restart from scratch - forget the preferred character.
	string str = shortcut->cleanShortcutString();

	for ( string::size_type pos = 0; pos < str.length(); pos++ )	// Search all the shortcut string.
	{
	    char c = YShortcut::normalized( str[ pos ] );
	    // y2debug( "Checking #%d '%c'", (int) c, c );

	    if ( YShortcut::isValid(c) && ! _used[ (int) c ] ) 	// Could we use this character?
	    {
		if ( _wanted[ (int) c ] < _wanted[ (int) candidate ]	// Is this a better choice than what we already have -
		     || ! YShortcut::isValid( candidate ) )		// or don't we have anything yet?
		{
		    candidate = c;			// Use this one.
		    // y2debug( "Picking %c", c );

		    if ( _wanted[ (int) c ] == 0 )	// It doesn't get any better than this:
			break;				// Nobody wants this shortcut anyway.
		}
	    }
	}
    }

    if ( YShortcut::isValid( candidate ) )
    {
	if ( candidate != shortcut->preferred() )
	{
	    if ( shortcut->widget()->autoShortcut() )
	    {
		y2debug( "Automatically assigning shortcut '%c' to %s(`opt(`autoShortcut ), \"%s\" )",
			 candidate, shortcut->widgetClass(), shortcut->cleanShortcutString().c_str() );
	    }
	    else
	    {
		y2debug( "Reassigning shortcut '%c' for %s \"%s\"",
			 candidate, shortcut->widgetClass(), shortcut->cleanShortcutString().c_str() );
	    }
	    shortcut->setShortcut( candidate );
	}
	else
	{
	    y2debug( "Keeping preferred shortcut '%c' for %s \"%s\"",
		     candidate, shortcut->widgetClass(), shortcut->cleanShortcutString().c_str() );
	}

	_used[ (int) candidate ] = true;
	shortcut->setConflict( false );
    }
    else	// No unique shortcut found
    {
	y2milestone( "Couldn't resolve shortcut conflict for %s \"%s\" - assigning no shortcut",
		     shortcut->widgetClass(), shortcut->cleanShortcutString().c_str() );
	shortcut->clearShortcut();
	shortcut->setConflict( false );
    }

    _conflictCount--;
}



int
YShortcutManager::findShortestWizardButton( const YShortcutList & conflictList )
{
    int shortestIndex = -1;
    int shortestLen   = -1;

    for ( unsigned i=1; i < conflictList.size(); i++ )
    {
	if ( conflictList[i]->isWizardButton() )
	{
	    if ( shortestLen < 0 ||
		 conflictList[i]->distinctShortcutChars() < shortestLen )
	    {
		shortestIndex = i;
		shortestLen   = conflictList[i]->distinctShortcutChars();
	    }

	}
    }

    return shortestIndex;
}



unsigned
YShortcutManager::findShortestWidget( const YShortcutList & conflictList )
{
    unsigned shortestIndex = 0;
    int      shortestLen   = conflictList[ shortestIndex ]->distinctShortcutChars();

    for ( unsigned i=1; i < conflictList.size(); i++ )
    {
	int currentLen = conflictList[i]->distinctShortcutChars();

	if ( currentLen < shortestLen )
	{
	    // Found an even shorter one

	    shortestIndex = i;
	    shortestLen   = currentLen;
	}
	else if ( currentLen == shortestLen )
	{
	    if ( conflictList[i]->isButton() &&
		 ! conflictList[ shortestIndex ]->isButton() )
	    {
		// Prefer a button over another widget with the same length

		shortestIndex = i;
		shortestLen   = currentLen;
	    }
	}
    }

    return shortestIndex;
}



void
YShortcutManager::clearShortcutList()
{
    for ( unsigned i=0; i < _shortcutList.size(); i++ )
    {
	delete _shortcutList[i];
    }

    _shortcutList.clear();
}


void
YShortcutManager::findShortcutWidgets( YWidgetListConstIterator begin,
				       YWidgetListConstIterator end )
{
    for ( YWidgetListConstIterator it = begin; it != end; ++it )
    {
	YWidget * widget = *it;
	
	if ( ! widget->shortcutString().empty() )
	{
	    YShortcut * shortcut = new YShortcut( *it );
	    _shortcutList.push_back( shortcut );
	}

	if ( widget->hasChildren() )
	{
	    findShortcutWidgets( widget->childrenBegin(),
				 widget->childrenEnd()   );
	}
    }
}


