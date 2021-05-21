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


#pragma once
#include "Configure.h"
#include "HXType.h"
#include "HXDefine.h"
#include "MetisGrid.h"
#include <vector>
#include <map>
using namespace std;

BeginNameSpace( ONEFLOW )

class ScalarIFaceIJ
{
public:
    ScalarIFaceIJ() ;
    ~ScalarIFaceIJ();
public:
    int zonei, zonej;
    vector< int > ghostCells;
    //global interface id
    vector< int > iglobalfaces;
    //local interface id
    vector< int > ifaces;
    vector< int > cells;

    vector< int > target_ifaces;
    vector< int > recv_ifaces;
};

class ScalarFacePair
{
public:
    ScalarFacePair();
    ~ScalarFacePair();
public:
    int zonei, zonej;
    vector< int > ifaces;
    vector< int > neibor_ifaces;
};

class GridTopo;
class GridTopos;
class DataStorage;

class ScalarIFace
{
public:
    ScalarIFace( int zoneid = 0 );
    ~ScalarIFace();
public:
    vector< ScalarIFaceIJ > data;
    vector< ScalarFacePair > sendinfo;
    vector< ScalarFacePair > recvinfo;
    
    vector<int> iglobalfaces;
    //targt zones
    vector<int> zones;
    //target cells
    vector<int> cells;
    //target interfaces (local)
    vector<int> target_interfaces;
    int zoneid;
    DataStorage * dataSend;
    DataStorage * dataRecv;
    //global interface id to local interface id map
    map<int, int> global_to_local_interfaces;
    //local interface id to global interface id map
    map<int, int> local_to_global_interfaces;
public:
    int GetNIFaces();
    int FindINeibor( int iZone );
    void DumpInterfaceMap();
    void DumpMap( map<int, int> & mapin );
    int GetLocalInterfaceId( int global_interface_id );
    void CalcLocalInterfaceId( int iZone, vector<int> & globalfaces, vector<int> & localfaces );
    void GetInterface();
    void AddInterface( int global_interface_id, int neighbor_zoneid, int neighbor_cellid );
    void ReconstructNeighbor();
    DataStorage * GetDataStorage( int iSendRecv );
};

class ScalarIFaces
{
public:
    ScalarIFaces() ;
    ~ScalarIFaces();
public:
    vector< ScalarIFace > data;
public:
    void GetInterface();
};

EndNameSpace