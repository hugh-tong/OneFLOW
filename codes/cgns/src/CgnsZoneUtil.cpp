/*---------------------------------------------------------------------------*\
    OneFLOW - LargeScale Multiphysics Scientific Simulation Environment
    Copyright (C) 2017-2020 He Xin and the OneFLOW contributors.
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

#include "CgnsZone.h"
#include "CgnsZoneUtil.h"
#include "CgnsBase.h"
#include "CgnsCoor.h"
#include "CgnsSection.h"
#include "CgnsZsection.h"
#include "CgnsBcBoco.h"
#include "CgnsZbc.h"
#include "CgnsZbcBoco.h"
#include "BcRecord.h"
#include "Boundary.h"
#include "NodeMesh.h"
#include "StrUtil.h"
#include "Grid.h"
#include "BgGrid.h"
#include "StrGrid.h"
#include "GridState.h"
#include "Dimension.h"
#include "GridElem.h"
#include "ElemFeature.h"
#include "ElementHome.h"
#include "PointFactory.h"
#include "PointSearch.h"
#include "FaceSolver.h"
#include "Stop.h"
#include <iostream>
#include <iomanip>
using namespace std;

BeginNameSpace( ONEFLOW )
#ifdef ENABLE_CGNS

void EncodeIJK( int & index, int i, int j, int k, int ni, int nj, int nk )
{
    index = ( i - 1 ) + ( j - 1 ) * ni + ( k - 1 ) * ( ni * nj ) ;
}

void DecodeIJK( int index, int & i, int & j, int & k, int ni, int nj, int nk )
{
    k = index / ( ni * nj ) + 1;
    index -= ( k - 1 ) * ni * nj;
    j = index / ni + 1;
    i = index - ( j - 1 ) * ni + 1;
}

void GetRange( int ni, int nj, int nk, int startShift, int endShift, Range & I, Range & J, Range & K )
{
    I.SetRange( 1 + startShift, ni + endShift );
    J.SetRange( 1 + startShift, nj + endShift );
    K.SetRange( 1 + startShift, nk + endShift );

    if ( ni == 1 ) I.SetRange( 1, 1 );
    if ( nj == 1 ) J.SetRange( 1, 1 );
    if ( nk == 1 ) K.SetRange( 1, 1 );
}

void GetIJKRegion( Range & I, Range & J, Range & K, int & ist, int & ied, int & jst, int & jed, int & kst, int & ked )
{
    ist = I.First();
    ied = I.Last();
    jst = J.First();
    jed = J.Last();
    kst = K.First();
    ked = K.Last();
}

void PrepareCgnsZone( Grids & grids, CgnsZone * cgnsZone )
{
    NodeMesh * nodeMesh = cgnsZone->cgnsCoor->GetNodeMesh();

    int nNode, nCell;

    HXVector< Int3D * > unsIdList;

    MergeToSingleZone( grids, unsIdList, nodeMesh, nNode, nCell );

    cgnsZone->cgnsCoor->SetNNode( nNode );
    cgnsZone->cgnsCoor->SetNCell( nCell );

    FillSection( grids, unsIdList, cgnsZone );

    cgnsZone->ConvertToInnerDataStandard();

    ONEFLOW::DeletePointer( unsIdList );
}

void MergeToSingleZone( Grids & grids, HXVector< Int3D * > & unsIdList, NodeMesh * nodeMesh, int & nNode, int & nCell )
{
    PointSearch * point_search = new PointSearch();
    point_search->Initialize( grids );

    size_t nZone = grids.size();

    unsIdList.resize( nZone );
    nCell = 0;
    for ( int iZone = 0; iZone < nZone; ++ iZone )
    {
        StrGrid * grid = ONEFLOW::StrGridCast( grids[ iZone ] );
        int ni = grid->ni;
        int nj = grid->nj;
        int nk = grid->nk;
        nCell += grid->nCell;
        unsIdList[ iZone ] = new Int3D( Range( 1, ni ), Range( 1, nj ), Range( 1, nk ) );
        Int3D & unsId = * unsIdList[ iZone ];
        cout << " block = " << iZone + 1 << "\n";
        ComputeUnsId( grid, point_search, & unsId );
    }

    nNode = point_search->GetNPoint();

    cout << " First nNode = " << nNode << "\n";
    nodeMesh->xN.resize( nNode );
    nodeMesh->yN.resize( nNode );
    nodeMesh->zN.resize( nNode );
    for ( int i = 0; i < nNode; ++ i )
    {
        Real xm, ym, zm;
        point_search->GetPoint( i, xm, ym, zm );

        nodeMesh->xN[ i ] = xm;
        nodeMesh->yN[ i ] = ym;
        nodeMesh->zN[ i ] = zm;
    }
    delete point_search;
}

void FillSection( Grids & grids, HXVector< Int3D * > & unsIdList, CgnsZone * cgnsZone )
{
    int nTBcRegion = 0;

    int nTCell = 0;
    int nBFace = 0;

    for ( int iZone = 0; iZone < grids.size(); ++ iZone )
    {
        StrGrid * grid = ONEFLOW::StrGridCast( grids[ iZone ] );
        Int3D & unsId = * unsIdList[ iZone ];

        nTCell += grid->ComputeNumberOfCell();

        BcRegionGroup * bcRegionGroup = grid->bcRegionGroup;
        size_t nBcRegions = bcRegionGroup->regions->size();

        for ( int ir = 0; ir < nBcRegions; ++ ir )
        {
            BcRegion * bcRegion = ( * bcRegionGroup->regions )[ ir ];
            if ( BC::IsNotNormalBc( bcRegion->bcType ) ) continue;
            
            nBFace += bcRegion->ComputeRegionCells();
            nTBcRegion ++;
        }
    }

    cout << " nBFace = " << nBFace << "\n";

    cgnsZone->cgnsCoor->SetNCell( nTCell );

    cgnsZone->cgnsZsection->nSection = 2;
    cgnsZone->cgnsZsection->CreateCgnsSection();

    cgnsZone->cgnsZsection->cgnsSections[ 0 ]->startId = 1;
    cgnsZone->cgnsZsection->cgnsSections[ 0 ]->endId   = nTCell;

    cgnsZone->cgnsZsection->cgnsSections[ 1 ]->startId = nTCell + 1;
    cgnsZone->cgnsZsection->cgnsSections[ 1 ]->endId   = nTCell + 1 + nBFace;

    if ( Dim::dimension == ONEFLOW::THREE_D )
    {
        cgnsZone->cgnsZsection->cgnsSections[ 0 ]->eType = HEXA_8;
        cgnsZone->cgnsZsection->cgnsSections[ 1 ]->eType = QUAD_4;
    }
    else
    {
        cgnsZone->cgnsZsection->cgnsSections[ 0 ]->eType = QUAD_4;
        cgnsZone->cgnsZsection->cgnsSections[ 1 ]->eType = BAR_2;
    }

    cgnsZone->cgnsZsection->CreateConnList();

    CgnsZbc * cgnsZbc = cgnsZone->cgnsZbc;
    cgnsZbc->cgnsZbcBoco->ReadZnboco( nTBcRegion );
    cgnsZbc->CreateCgnsZbc( cgnsZbc );

    CgnsSection * secV = cgnsZone->cgnsZsection->GetCgnsSection( 0 );
    CgnsSection * secB = cgnsZone->cgnsZsection->GetCgnsSection( 1 );

    CgIntField& connList  = secV->connList;
    CgIntField& bConnList = secB->connList;

    int pos = 0;

    for ( int iZone = 0; iZone < grids.size(); ++ iZone )
    {
        StrGrid * grid = ONEFLOW::StrGridCast( grids[ iZone ] );
        int ni = grid->ni;
        int nj = grid->nj;
        int nk = grid->nk;

        Int3D & unsId = * unsIdList[ iZone ];
        
        IJKRange::Compute( ni, nj, nk, 0, -1 );
        IJKRange::ToScalar();

        int is = 1;
        int js = 1;
        int ks = 1;

        if ( Dim::dimension == ONEFLOW::TWO_D ) ks = 0;

        int eNodeNumbers = ONEFLOW::GetElementNodeNumbers( secV->eType );

        for ( int k = IJKRange::kst; k <= IJKRange::ked; ++ k )
        {
            for ( int j = IJKRange::jst; j <= IJKRange::jed; ++ j )
            {
                for ( int i = IJKRange::ist; i <= IJKRange::ied; ++ i )
                {
                    connList[ pos + 0 ] = unsId( i   , j   , k    ) + 1;
                    connList[ pos + 1 ] = unsId( i+is, j   , k    ) + 1;
                    connList[ pos + 2 ] = unsId( i+is, j+js, k    ) + 1;
                    connList[ pos + 3 ] = unsId( i   , j+js, k    ) + 1;
                    if ( Dim::dimension == ONEFLOW::THREE_D )
                    {
                        connList[ pos + 4 ] = unsId( i   , j   , k+ks ) + 1;
                        connList[ pos + 5 ] = unsId( i+is, j   , k+ks ) + 1;
                        connList[ pos + 6 ] = unsId( i+is, j+js, k+ks ) + 1;
                        connList[ pos + 7 ] = unsId( i   , j+js, k+ks ) + 1;
                    }
                    pos += eNodeNumbers;
                }
            }
        }
    }

    secV->SetElemPosition();
    secB->SetElemPosition();

    int irc  = 0;
    int eIdPos  = nTCell;
    pos = 0;

    BcTypeMap * bcTypeMap = new BcTypeMap();
    bcTypeMap->Init();

    for ( int iZone = 0; iZone < grids.size(); ++ iZone )
    {
        StrGrid * grid = ONEFLOW::StrGridCast( grids[ iZone ] );
        int ni = grid->ni;
        int nj = grid->nj;
        int nk = grid->nk;

        Int3D & unsId = * unsIdList[ iZone ];

        BcRegionGroup * bcRegionGroup = grid->bcRegionGroup;
        size_t nBcRegions = bcRegionGroup->regions->size();

        for ( int ir = 0; ir < nBcRegions; ++ ir )
        {
            BcRegion * bcRegion = ( * bcRegionGroup->regions )[ ir ];
            if ( BC::IsNotNormalBc( bcRegion->bcType ) ) continue;
            int nRegionCell = bcRegion->ComputeRegionCells();

            CgnsBcBoco * cgnsBcBoco = cgnsZbc->cgnsZbcBoco->GetCgnsBc( irc );
            
            cgnsBcBoco->SetCgnsBcRegionGridLocation( CellCenter );
            cgnsBcBoco->nElements    = 2;
            cgnsBcBoco->bcType       = static_cast< BCType_t >( bcTypeMap->OneFlow2Cgns( bcRegion->bcType ) );
            cgnsBcBoco->pointSetType = PointRange;

            //cgnsBcBoco->SetCgnsBcRegion( nElements, bcType, );

            cgnsBcBoco->CreateCgnsBcConn();
            cgnsBcBoco->connList[ 0 ] = eIdPos + 1;
            cgnsBcBoco->connList[ 1 ] = eIdPos + nRegionCell;
            string bcName = GetCgnsBcName( cgnsBcBoco->bcType );
            cgnsBcBoco->name = AddString( bcName, ir );

            eIdPos += nRegionCell;

            ONEFLOW::SetUnsBcConn( bcRegion, bConnList, pos, unsId );

            irc ++;
        }
    }

    delete bcTypeMap;
}

void ComputeUnsId( StrGrid * grid, PointSearch * pointSearch, Int3D * unsId )
{
    int ni = grid->ni;
    int nj = grid->nj;
    int nk = grid->nk;

    Field3D & xs = * grid->strx;
    Field3D & ys = * grid->stry;
    Field3D & zs = * grid->strz;

    RealField coordinate( 3 );
    for ( int k = 1; k <= nk; ++ k )
    {
        for ( int j = 1; j <= nj; ++ j )
        {
            for ( int i = 1; i <= ni; ++ i )
            {
                Real xm = xs( i, j, k );
                Real ym = ys( i, j, k );
                Real zm = zs( i, j, k );

                int pointIndex = pointSearch->AddPoint( xm, ym, zm );

                //{
                //    int width = 8;
                //    cout << " id = " << pointIndex;
                //    cout << setw( width ) << xm;
                //    cout << setw( width ) << ym;
                //    cout << setw( width ) << zm;
                //    cout << "\n";
                //}
                

                ( * unsId )( i, j, k ) = pointIndex;
            }
        }
    }
}

void SetUnsBcConn( BcRegion * bcRegion, CgIntField& conn, int & pos, Int3D & unsId )
{
    int ist, ied, jst, jed, kst, ked;
    bcRegion->GetNormalizeIJKRegion( ist, ied, jst, jed, kst, ked );

    cout << " ist, ied, jst, jed, kst, ked = " << ist << " " << ied << " " << jst << " " << jed << " " << kst << " " << ked << endl;
    int numpt = 4;
    if ( Dim::dimension == TWO_D ) numpt = 2;

    if ( ist == ied )
    {
        int i = ist;
        if ( Dim::dimension == THREE_D )
        {
            for ( int k = kst; k <= ked - 1; ++ k )
            {
                for ( int j = jst; j <= jed - 1; ++ j )
                {
                    if ( i == 1 )
                    {
                        conn[ pos + 0 ] = unsId( i, j    , k     ) + 1;
                        conn[ pos + 1 ] = unsId( i, j    , k + 1 ) + 1;
                        conn[ pos + 2 ] = unsId( i, j + 1, k + 1 ) + 1;
                        conn[ pos + 3 ] = unsId( i, j + 1, k     ) + 1;
                    }
                    else
                    {
                        conn[ pos + 0 ] = unsId( i, j    , k     ) + 1;
                        conn[ pos + 1 ] = unsId( i, j + 1, k     ) + 1;
                        conn[ pos + 2 ] = unsId( i, j + 1, k + 1 ) + 1;
                        conn[ pos + 3 ] = unsId( i, j    , k + 1 ) + 1;
                    }
                    pos += numpt;
                }
            }
        }
        else
        {
            int k = kst;
            for ( int j = jst; j <= jed - 1; ++ j )
            {
                if ( i == 1 )
                {
                    conn[ pos + 0 ] = unsId( i, j + 1, k  ) + 1;
                    conn[ pos + 1 ] = unsId( i, j    , k  ) + 1;
                }
                else
                {
                    conn[ pos + 0 ] = unsId( i, j    , k  ) + 1;
                    conn[ pos + 1 ] = unsId( i, j + 1, k  ) + 1;
                }
                pos += numpt;
            }
        }
        return;
    }

    if ( jst == jed )
    {
        int j = jst;
        if ( Dim::dimension == THREE_D )
        {
            for ( int k = kst; k <= ked - 1; ++ k )
            {
                for ( int i = ist; i <= ied - 1; ++ i )
                {
                    if ( j == 1 )
                    {
                        conn[ pos + 0 ] = unsId( i    , j, k    ) + 1;
                        conn[ pos + 1 ] = unsId( i + 1, j, k    ) + 1;
                        conn[ pos + 2 ] = unsId( i + 1, j, k + 1 ) + 1;
                        conn[ pos + 3 ] = unsId( i    , j, k + 1 ) + 1;
                    }   
                    else
                    {
                        conn[ pos + 0 ] = unsId( i    , j, k     ) + 1;
                        conn[ pos + 1 ] = unsId( i    , j, k + 1 ) + 1;
                        conn[ pos + 2 ] = unsId( i + 1, j, k + 1 ) + 1;
                        conn[ pos + 3 ] = unsId( i + 1, j, k     ) + 1;
                    }
                    pos += numpt;
                }
            }
        }
        else
        {
            int k = kst;
            for ( int i = ist; i <= ied - 1; ++ i )
            {
                if ( j == 1 )
                {
                    conn[ pos + 0 ] = unsId( i    , j, k  ) + 1;
                    conn[ pos + 1 ] = unsId( i + 1, j, k  ) + 1;
                }   
                else
                {
                    conn[ pos + 0 ] = unsId( i + 1, j, k  ) + 1;
                    conn[ pos + 1 ] = unsId( i    , j, k  ) + 1;
                }
                pos += numpt;
            }
        }
        return;
    }

    if ( kst == ked )
    {
        int k = kst;
        for ( int j = jst; j <= jed - 1; ++ j )
        {
            for ( int i = ist; i <= ied - 1; ++ i )
            {
                if ( k == 1 )
                {
                    conn[ pos + 0 ] = unsId( i    , j    , k ) + 1;
                    conn[ pos + 1 ] = unsId( i    , j + 1, k ) + 1;
                    conn[ pos + 2 ] = unsId( i + 1, j + 1, k ) + 1;
                    conn[ pos + 3 ] = unsId( i + 1, j    , k ) + 1;
                }   
                else
                {
                    conn[ pos + 0 ] = unsId( i    , j    , k ) + 1;
                    conn[ pos + 1 ] = unsId( i + 1, j    , k ) + 1;
                    conn[ pos + 2 ] = unsId( i + 1, j + 1, k ) + 1;
                    conn[ pos + 3 ] = unsId( i    , j + 1, k ) + 1;
                }
                pos += numpt;
            }
        }
        return;
    }

    Stop( " error : ist != ied, jst != jed, kst != ked \n" );
}

void GenerateUnsBcElemConn( CgnsZone * myZone, CgnsZone * cgnsZoneIn )
{
    int iSection = 1;
    CgnsSection * cgnsSection = myZone->cgnsZsection->GetCgnsSection( iSection );

    myZone->cgnsZbc->CreateCgnsZbc( cgnsZoneIn->cgnsZbc );

    cout << " ConnectionList Size = " << cgnsSection->connSize << "\n";
    cgnsZoneIn->cgnsZbc->GenerateUnsBcElemConn( cgnsSection->connList );
}

void GenerateUnsBcCondConn( CgnsZone * myZone, CgnsZone * cgnsZoneIn )
{
    int iSection = 1;
    CgnsSection * cgnsSection = myZone->cgnsZsection->GetCgnsSection( iSection );

    CgInt startId = cgnsSection->startId;

    int nBoco = cgnsZoneIn->cgnsZbc->cgnsZbcBoco->nBoco;
    for ( int iBoco = 0; iBoco < nBoco; ++ iBoco )
    {
        CgnsBcBoco * bcRegion    = myZone    ->cgnsZbc->cgnsZbcBoco->GetCgnsBc( iBoco );
        CgnsBcBoco * strBcRegion = cgnsZoneIn->cgnsZbc->cgnsZbcBoco->GetCgnsBc( iBoco );
        bcRegion->CopyStrBcRegion( strBcRegion, startId );
    }
}


#endif
EndNameSpace