/*---------------------------------------------------------------------------*\
    OneFLOW - LargeScale Multiphysics Scientific Simulation Environment
    Copyright (C) 2017-2021 He Xin and the OneFLOW contributors.
-------------------------------------------------------------------------------
License
    This file is part of OneFLOW.

    OneFLOW is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OneFLOW is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OneFLOW.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "ScalarIFace.h"
#include "MetisGrid.h"
#include "DataStorage.h"
#include "SolverDef.h"
#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

BeginNameSpace( ONEFLOW )

ScalarIFaceIJ::ScalarIFaceIJ()
{
    ;
}

ScalarIFaceIJ::~ScalarIFaceIJ()
{
}

ScalarFacePair::ScalarFacePair()
{
    ;
}

ScalarFacePair::~ScalarFacePair()
{
}

ScalarIFace::ScalarIFace( int zoneid )
{
    this->zoneid = zoneid;
    this->dataSend = new DataStorage();
    this->dataRecv = new DataStorage();
}

ScalarIFace::~ScalarIFace()
{
    delete this->dataSend;
    delete this->dataRecv;
}

void ScalarIFace::AddInterface( int global_interface_id, int neighbor_zoneid, int neighbor_cellid )
{
    int ilocal_interface = this->iglobalfaces.size();
    this->iglobalfaces.push_back( global_interface_id );
    this->zones.push_back( neighbor_zoneid );
    this->cells.push_back( neighbor_cellid );
    this->global_to_local_interfaces[ global_interface_id ] = ilocal_interface;
    this->local_to_global_interfaces[ ilocal_interface ] = global_interface_id;
}

int ScalarIFace::GetLocalInterfaceId( int global_interface_id )
{
    return this->global_to_local_interfaces[ global_interface_id ];
}

int ScalarIFace::GetNIFaces()
{
    return zones.size();
}

int ScalarIFace::FindINeibor( int iZone )
{
    int nNeis = data.size();
    for( int iNei = 0; iNei < nNeis; ++ iNei)
    {
        ScalarIFaceIJ & iFaceIJ = data[ iNei ];
        if ( iZone == iFaceIJ.zonej )
        {
            return iNei;
        }
    }
    return -1;
}

void ScalarIFace::CalcLocalInterfaceId( int iZone, vector<int> & globalfaces, vector<int> & localfaces )
{
    for ( int i = 0; i < globalfaces.size(); ++ i )
    {
        int gid = globalfaces[ i ];
        int lid = this->global_to_local_interfaces[ gid ];
        localfaces.push_back( lid );
    }
    //The neighbor of iZone iNei is jzone, and the jNei neighbor of jZone is iZone
    int jNei = FindINeibor( iZone );
    cout << " zoneid = " << this->zoneid << " iZone() = " << iZone << " jNei = " << jNei << "\n";
    ScalarIFaceIJ & iFaceIJ = this->data[ jNei ];
    iFaceIJ.recv_ifaces = localfaces;
}

void ScalarIFace::DumpInterfaceMap()
{
    cout << " global_to_local_interfaces map \n";
    this->DumpMap( this->global_to_local_interfaces );
    cout << "\n";
    cout << " local_to_global_interfaces map \n";
    this->DumpMap( this->local_to_global_interfaces );
    cout << "\n";

    //for ( int i = 0; i < zones.size(); ++ i )
    //{
    //    cout << " zone: " << zones[ i ] << " iface = " << target_interfaces[ i ] << " ";
    //}
    //cout << "\n";
}

void ScalarIFace::DumpMap( map<int,int> & mapin )
{
    for ( map<int, int>::iterator iter = mapin.begin(); iter != mapin.end(); ++ iter )
    {
        cout << iter->first << " " << iter->second << "\n";
    }
    cout << "\n";
}

void ScalarIFace::GetInterface()
{
    int nNeis = data.size();
    for( int iNei = 0; iNei < nNeis; ++ iNei)
    {
        ScalarIFaceIJ & iFaceIJ = data[ iNei ];
        int iZone =  iFaceIJ.zonej;
        vector<int> & ghostCells = iFaceIJ.ghostCells;
    }
}

void ScalarIFace::ReconstructNeighbor()
{
    int nSize = zones.size();
    set<int> nei_zoneidset;
    for ( int i = 0; i < nSize; ++ i )
    {
        int nei_zoneid = zones[ i ];
        nei_zoneidset.insert( nei_zoneid );
    }

    for ( set<int>::iterator iter = nei_zoneidset.begin(); iter != nei_zoneidset.end(); ++ iter )
    {
        ScalarIFaceIJ sij;
        int current_nei_zoneid = * iter;
        sij.zonei = zoneid;
        sij.zonej = current_nei_zoneid;

        for ( int i = 0; i < nSize; ++ i )
        {
            int nei_zoneid = zones[ i ];
            if ( nei_zoneid == current_nei_zoneid )
            {
                sij.cells.push_back( this->cells[ i ] );
                sij.iglobalfaces.push_back( this->iglobalfaces[ i ] );
                sij.ifaces.push_back( i );
            }
        }
        this->data.push_back( sij );
    }
}

DataStorage * ScalarIFace::GetDataStorage( int iSendRecv )
{
    if ( iSendRecv == SEND_STORAGE )
    {
        return this->dataSend;
    }
    else if ( iSendRecv == RECV_STORAGE )
    {
        return this->dataRecv;
    }
    else
    {
        return 0;
    }
}


ScalarIFaces::ScalarIFaces()
{
    ;
}

ScalarIFaces::~ScalarIFaces()
{
    ;
}

void ScalarIFaces::GetInterface()
{
    int nZones = data.size();
    for( int iZone = 0; iZone < nZones; ++ iZone )
    {
        data[ iZone ].GetInterface();
    }
}

EndNameSpace