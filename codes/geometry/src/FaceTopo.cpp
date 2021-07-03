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

#include "FaceTopo.h"
#include "BcRecord.h"
#include "Boundary.h"
#include "IFaceLink.h"
#include "InterFace.h"
#include "FaceSearch.h"
#include "Grid.h"
#include "HXMath.h"
#include <iostream>
#include <algorithm>
using namespace std;

BeginNameSpace( ONEFLOW )

FaceTopo::FaceTopo()
{
    grid = 0;

    bcManager = new BcManager();
}

FaceTopo::~FaceTopo()
{
    delete bcManager;
}

UInt FaceTopo::CalcTotalFaceNodes()
{
    UInt totalNumFaceNodes = 0;
    UInt nFaces = this->GetNFace();
    for ( UInt iFace = 0; iFace < nFaces; ++ iFace )
    {
        totalNumFaceNodes += faces[ iFace ].size();
    }
    return totalNumFaceNodes;
}

UInt FaceTopo::GetNBFace()
{
    return this->bcManager->bcRecord->GetNBFace();
}

void FaceTopo::SetNBFace( UInt nBFace )
{
    this->bcManager->bcRecord->Init( nBFace );
}

void FaceTopo::ModifyFaceNodeId( IFaceLink * iFaceLink )
{
    this->SetNewFace2Node( iFaceLink );
    this->SetNewFace2Cell( iFaceLink );
}

void FaceTopo::SetNewFace2Node( IFaceLink * iFaceLink )
{
    int nBFace = this->bcManager->bcRecord->GetNBFace();
    this->facesNew.resize( 0 );

    int localFid = 0;
    for ( int iFace = 0; iFace < nBFace; ++ iFace )
    {
        int bcType = this->bcManager->bcRecord->bcType[ iFace ];

        int nFNode = this->faces[ iFace ].size();
        IntField tmpVector;

        if ( BC::IsInterfaceBc( bcType ) )
        {
            int gFid   = iFaceLink->l2g[ this->grid->id ][ localFid ];
            int nCFace = iFaceLink->face_search->cFaceId[ gFid ].size();

            if ( nCFace > 0 )
            {
                for ( int iCFace = 0; iCFace < nCFace; ++ iCFace )
                {
                    //this->lCellNew.push_back( this->lCell[ iFace ] );

                    int cFid = iFaceLink->face_search->cFaceId[ gFid ][ iCFace ];

                    int nCNode = iFaceLink->face_search->rCNodeId[ cFid ].size();

                    tmpVector.resize( 0 );

                    for ( int iNode = 0; iNode < nCNode; ++ iNode )
                    {
                        int flag = iFaceLink->face_search->rCNodeFlag[ cFid ][ iNode ];
                        int nodeIndex;
                        if ( flag == 1 )
                        {
                            int rNId = iFaceLink->face_search->rCNodeId[ cFid ][ iNode ];
                            nodeIndex = this->faces[ iFace ][ rNId ];
                        }
                        else
                        {
                            //At this time, the storage is not a relative value, but an absolute new punctuation
                            nodeIndex = iFaceLink->face_search->rCNodeId[ cFid ][ iNode ];
                        }
                        tmpVector.push_back( nodeIndex );
                    }
                    this->facesNew.push_back( tmpVector );
                }
            }
            else
            {
                for ( int iNode = 0; iNode < nFNode; ++ iNode )
                {
                    int nodeIndex = this->faces[ iFace ][ iNode ];
                    tmpVector.push_back( nodeIndex );
                }
                this->facesNew.push_back( tmpVector );
            }
            ++ localFid;
        }
        else
        {
              for ( int iNode = 0; iNode < nFNode; ++ iNode )
            {
                int nodeIndex = this->faces[ iFace ][ iNode ];
                tmpVector.push_back( nodeIndex );
            }
            this->facesNew.push_back( tmpVector );
        }
    }

    //Inner Face
    int nFaces = this->GetNFace();
    for ( int iFace = nBFace; iFace < nFaces; ++ iFace )
    {
        int nFNode = this->faces[ iFace ].size();
        IntField tmpVector;
        for ( int iNode = 0; iNode < nFNode; ++ iNode )
        {
            int nId = this->faces[ iFace ][ iNode ];
            tmpVector.push_back( nId );
        }
        this->facesNew.push_back( tmpVector );
    }
}

void FaceTopo::SetNewFace2Cell( IFaceLink * iFaceLink )
{
    int nBFace = this->bcManager->bcRecord->GetNBFace();

    int localFid = 0;

    this->lCellNew.resize( 0 );
    this->rCellNew.resize( 0 );

    for ( int iFace = 0; iFace < nBFace; ++ iFace )
    {
        int bcType = this->bcManager->bcRecord->bcType[ iFace ];

        if ( BC::IsInterfaceBc( bcType ) )
        {
            int gFid   = iFaceLink->l2g[ this->grid->id ][ localFid ];
            int nCFace = iFaceLink->face_search->cFaceId[ gFid ].size();

            if ( nCFace > 0 )
            {
                for ( int iChildFace = 0; iChildFace < nCFace; ++ iChildFace )
                {
                    this->lCellNew.push_back( this->lCell[ iFace ] );
                }
            }
            else
            {
                this->lCellNew.push_back( this->lCell[ iFace ] );
            }
            ++ localFid;
        }
        else
        {
            this->lCellNew.push_back( this->lCell[ iFace ] );
        }
    }

    int nBFaceNew = this->lCellNew.size();

    int nCells = this->grid->nCells;

    for ( int iFace = 0; iFace < nBFaceNew; ++ iFace )
    {
        this->rCellNew.push_back( iFace + nCells );
    }

    int nFaces = this->GetNFace();

    for ( int iFace = nBFace; iFace < nFaces; ++ iFace )
    {
        this->lCellNew.push_back( this->lCell[ iFace ] );
        this->rCellNew.push_back( this->rCell[ iFace ] );
    }
}


void FaceTopo::ModifyBoundaryInformation( IFaceLink * iFaceLink )
{
    //find the new number of boundary faces
    int nBFace = this->bcManager->bcRecord->GetNBFace();

    int localIid = 0;

    IntField localI2B;

    for ( int iFace = 0; iFace < nBFace; ++ iFace )
    {
        int bcType = this->bcManager->bcRecord->bcType[ iFace ];
        if ( BC::IsInterfaceBc( bcType ) )
        {
            localI2B.push_back( iFace );
            ++ localIid;
        }
    }

    int nBFaceNew = 0;

    int nIFace = localI2B.size();

    int nIFaceNew = nIFace;

    iFaceLink->nChild.resize( nIFace, 0 );

    for ( int iFid = 0; iFid < nIFace; ++ iFid )
    {
        int gFid   = iFaceLink->l2g[ this->grid->id ][ iFid ];
        int nCFace = iFaceLink->face_search->cFaceId[ gFid ].size();

        if ( nCFace > 0 )
        {
            iFaceLink->nChild[ this->grid->id ][ iFid ] = nCFace;
            for ( int iCFace = 0; iCFace < nCFace; ++ iCFace )
            {
                int cFid = iFaceLink->face_search->cFaceId[ gFid ][ iCFace ];
                iFaceLink->l2gNew.push_back( cFid );
                iFaceLink->nChild[ this->grid->id ].push_back( 0 );
            }
            ++ nIFaceNew;
        }
        else
        {
            iFaceLink->l2gNew.push_back( gFid );
        }
    }
    cout << "original number of interfaces = " << nIFace << " new number of interfaces = " << nIFaceNew << endl;

    this->ResetNumberOfBoundaryCondition( iFaceLink );
}

void FaceTopo::ResetNumberOfBoundaryCondition( IFaceLink * iFaceLink )
{
    int nBFace = this->bcManager->bcRecord->GetNBFace();

    this->bcManager->bcRecordNew->bcType.resize( 0 );
    this->bcManager->bcRecordNew->bcNameId.resize( 0 );

    int localIid = 0;
    for ( int iFace = 0; iFace < nBFace; ++ iFace )
    {
        int bcRegion = this->bcManager->bcRecord->bcNameId[ iFace ];
        int bcType = this->bcManager->bcRecord->bcType[ iFace ];
        if ( BC::IsInterfaceBc( bcType ) )
        {
            int gFid   = iFaceLink->l2g[ this->grid->id ][ localIid ];
            int nCFace = iFaceLink->face_search->cFaceId[ gFid ].size();

            if ( nCFace > 0 )
            {
                for ( int iCFace = 0; iCFace < nCFace; ++ iCFace )
                {
                    this->bcManager->bcRecordNew->bcType.push_back( bcType );
                    this->bcManager->bcRecordNew->bcNameId.push_back( bcRegion );
                }
            }
            else
            {
                this->bcManager->bcRecordNew->bcType.push_back( bcType );
                this->bcManager->bcRecordNew->bcNameId.push_back( bcRegion );
            }

            ++ localIid;
        }
        else
        {
            this->bcManager->bcRecordNew->bcType.push_back( bcType );
            this->bcManager->bcRecordNew->bcNameId.push_back( bcRegion );
        }
    }

    this->ConstructNewInterfaceMap( iFaceLink );
}

void FaceTopo::ConstructNewInterfaceMap( IFaceLink * iFaceLink )
{
    int nIFaceNew = iFaceLink->l2gNew[ this->grid->id ].size();

    for ( int localIFid = 0; localIFid < nIFaceNew; ++ localIFid )
    {
        int gIFid = iFaceLink->l2gNew[ this->grid->id ][ localIFid ];

        int oldSize = iFaceLink->gI2ZidNew.size();
        int newSize = ONEFLOW::MAX( gIFid + 1, oldSize );

        iFaceLink->gI2ZidNew.resize( newSize );
        iFaceLink->g2lNew.resize( newSize );

        int nFZone = iFaceLink->gI2ZidNew[ gIFid ].size();

        if ( nFZone < 2 )
        {
            iFaceLink->gI2ZidNew[ gIFid ].push_back( this->grid->id );
            iFaceLink->g2lNew   [ gIFid ].push_back( localIFid );
        }
    }
}

void FaceTopo::UpdateOtherTopologyTerm()
{
    this->bcManager->Update();
    this->lCell = this->lCellNew;
    this->rCell = this->rCellNew;
    this->faces = this->facesNew;
}

void FaceTopo::GenerateI2B( InterFace * interFace )
{
    this->bcManager->bcRecord->GenerateI2B( interFace );
}

bool FaceTopo::GetSId( int iFace, int iPosition, int & sId )
{
    int iBFace = grid->interFace->i2b[ iFace ];
    sId = this->lCell[ iBFace ];

    return true;
}

bool FaceTopo::GetTId( int iFace, int iPosition, int & tId )
{
    int iBFace = grid->interFace->i2b[ iFace ];
    tId = this->rCell[ iBFace ];

    return true;
}

void FaceTopo::CalcC2C( LinkField & c2c )
{
    if ( c2c.size() != 0 ) return;

    int nCells = this->grid->nCells;
    int nBFace = this->GetNBFace();
    int nFaces = this->GetNFace();

    c2c.resize( nCells );

    // If boundary is an INTERFACE, need to count ghost cell
    for ( int iFace = 0; iFace < nBFace; ++ iFace )
    {
        int bcType = this->bcManager->bcRecord->bcType[ iFace ];
        if ( BC::IsInterfaceBc( bcType ) )
        {
            int lc  = this->lCell[ iFace ];
            int rc  = this->rCell[ iFace ];
            c2c[ lc  ].push_back( rc );
        }
    }

    for ( int iFace = nBFace; iFace < nFaces; ++ iFace )
    {
        int lc  = this->lCell[ iFace ];
        int rc  = this->rCell[ iFace ];
        c2c[ lc ].push_back( rc );
        c2c[ rc ].push_back( lc );
    }
}

EndNameSpace