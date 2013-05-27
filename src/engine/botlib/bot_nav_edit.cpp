/*
===========================================================================

Daemon GPL Source Code
Copyright (C) 2012 Unvanquished Developers

This file is part of the Daemon GPL Source Code (Daemon Source Code).

Daemon Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Daemon Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Daemon Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Daemon Source Code is also subject to certain additional terms.
You should have received a copy of these additional terms immediately following the
terms and conditions of the GNU General Public License which accompanied the Daemon
Source Code.  If not, please request a copy in writing from id Software at the address
below.

If you have questions concerning this license or the applicable additional terms, you
may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville,
Maryland 20850 USA.

===========================================================================
*/

extern "C" {
#include "../client/client.h"
}
#include "../../libs/detour/DetourDebugDraw.h"
#include "../../libs/detour/DebugDraw.h"
#include "bot_navdraw.h"
#include "nav.h"

qboolean GetPointPointedTo( NavData_t *nav, vec3_t p )
{
	vec3_t forward;
	vec3_t end;
	trace_t trace;
	vec3_t extents = { 640, 96, 640 };
	dtPolyRef nearRef;

	AngleVectors( cl.snap.ps.viewangles, forward, NULL, NULL );
	VectorMA( cl.snap.ps.origin, 8096, forward, end );

	CM_BoxTrace( &trace, cl.snap.ps.origin, end, NULL, NULL, 0, CONTENTS_SOLID | CONTENTS_PLAYERCLIP, TT_AABB );

	VectorCopy( trace.endpos, p );
	quake2recast( p );

	if ( dtStatusFailed( nav->query->findNearestPoly( p, extents, &nav->filter, &nearRef, end ) ) )
	{
		return qfalse;
	}

	VectorCopy( end, p );
	return qtrue;
}

struct
{
	bool enabled;
	bool offBegin;
	OffMeshConnection pc;
	NavData_t *nav;
} cmd;

void BotDrawNavEdit( DebugDrawQuake *dd )
{
	vec3_t p;

	if ( cmd.enabled && GetPointPointedTo( cmd.nav, p ) )
	{
		unsigned int col = duRGBA( 255, 255, 255, 220 );
		dd->begin( DU_DRAW_LINES, 2.0f );
		duAppendCircle( dd, p[ 0 ], p[ 1 ], p[ 2 ], 50, col );

		if ( cmd.offBegin )
		{
			duAppendArc( dd, cmd.pc.start[ 0 ], cmd.pc.start[ 1 ], cmd.pc.start[ 2 ], p[ 0 ], p[ 1 ], p[ 2 ], 0.25f,
						0.6f, 0.6f, col );
			duAppendCircle( dd, cmd.pc.start[ 0 ], cmd.pc.start[ 1 ], cmd.pc.start[ 2 ], 50, col );
		}
		dd->end();
	}
}

extern "C" void BotDebugDrawMesh( BotDebugInterface_t *in )
{
	static DebugDrawQuake dd;

	dd.init( in );

	if ( !cmd.enabled )
	{
		return;
	}

	if ( !cmd.nav )
	{
		return;
	}

	if ( !cmd.nav->mesh || !cmd.nav->query )
	{
		return;
	}

	duDebugDrawNavMeshWithClosedList(&dd, *cmd.nav->mesh, *cmd.nav->query, DU_DRAWNAVMESH_OFFMESHCONS | DU_DRAWNAVMESH_CLOSEDLIST);
	BotDrawNavEdit( &dd );
}

void Cmd_NavEdit( void )
{
	int argc = Cmd_Argc();
	char *arg = NULL;
	const char usage[] = "Usage: navedit enable/disable/save <navmesh>\n";

	if ( !Cvar_VariableIntegerValue( "sv_cheats" ) )
	{
		Com_Printf( "navedit only available in local devmap\n" );
		return;
	}

	arg = Cmd_Argv( 1 );

	if ( !Q_stricmp( arg, "enable" ) )
	{
		if ( argc < 3 )
		{
			Com_Printf( "%s", usage );
			return;
		}

		arg = Cmd_Argv( 1 );
		for ( int i = 0; i < numNavData; i++ )
		{
			if ( !Q_stricmp( BotNavData[ i ].name, arg ) )
			{
				cmd.nav = &BotNavData[ i ];
				break;
			}
		}

		if ( cmd.nav && cmd.nav->mesh && cmd.nav->cache && cmd.nav->query )
		{
			cmd.enabled = true;
			Cvar_Set( "r_debugSurface", "1" );
		}
	}
	else if ( !Q_stricmp( arg, "disable" ) )
	{
		cmd.enabled = false;
		Cvar_Set( "r_debugSurface", "0" );
	}
	else if ( !Q_stricmp( arg, "save" ) )
	{
		if ( !cmd.enabled )
		{
			return;
		}

		BotSaveOffMeshConnections( cmd.nav );
	}
}

void Cmd_AddConnection( void )
{
	const char usage[] = "Usage: addcon start/end";
	char *arg = NULL;
	int argc = Cmd_Argc();

	if ( argc < 2 )
	{
		Com_Printf( "%s", usage );
	}

	arg = Cmd_Argv( 1 );

	if ( !Q_stricmp( arg, "start" ) )
	{
		if ( !cmd.enabled )
		{
			return;
		}

		if ( GetPointPointedTo( cmd.nav, cmd.pc.start ) )
		{
			cmd.pc.area = DT_TILECACHE_WALKABLE_AREA;
			cmd.pc.flag = POLYFLAGS_WALK;
			cmd.pc.userid = 0;
			cmd.pc.radius = 50;
			cmd.pc.dir = 0;
			cmd.offBegin = true;
		}
	}
	else if ( !Q_stricmp( arg, "end" ) )
	{
		if ( !cmd.enabled )
		{
			return;
		}

		if ( !cmd.offBegin )
		{
			return;
		}
		
		if ( GetPointPointedTo( cmd.nav, cmd.pc.end ) )
		{
			cmd.nav->process.con.addConnection( cmd.pc );

			vec3_t omin, omax;
			ClearBounds( omin, omax );
			AddPointToBounds( cmd.pc.start, omin, omax );
			AddPointToBounds( cmd.pc.end, omin, omax );

			omin[ 1 ] -= 10;
			omax[ 1 ] += 10;

			// rebuild affected tiles
			dtCompressedTileRef refs[ 32 ];
			int tc = 0;
			cmd.nav->cache->queryTiles( omin, omax, refs, &tc, 32 );

			for ( int k = 0; k < tc; k++ )
			{
				cmd.nav->cache->buildNavMeshTile( refs[ k ], cmd.nav->mesh );
			}

			cmd.offBegin = false;
		}
	}
}

void NavEditInit( void )
{
#ifndef DEDICATED
	memset( &cmd, 0, sizeof( cmd ) );
	Cvar_Set( "r_debugSurface", "0" );
	Cmd_AddCommand( "navedit", Cmd_NavEdit );
	Cmd_AddCommand( "addcon", Cmd_AddConnection );
#endif
}

void NavEditShutdown( void )
{
#ifndef DEDICATED
	Cmd_RemoveCommand( "navedit" );
	Cmd_RemoveCommand( "addcon" );
#endif
}
