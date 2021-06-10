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

#include "ScalarGrid.h"
#include "Constant.h"
#include "HXCgns.h"
#include "Dimension.h"
#include "ElementHome.h"
#include "HXSort.h"
#include "HXMath.h"
#include "HXMathExt.h"
#include "Boundary.h"
#include "MetisGrid.h"
#include "ScalarIFace.h"
#include "DataBase.h"
#include "DataBook.h"
#include "Prj.h"
#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

BeginNameSpace( ONEFLOW )

RealList::RealList()
{
	;
}

RealList::~RealList()
{
	;
}

size_t RealList::GetNElements()
{
	return data.size();
}

void RealList::AddData( Real value )
{
	data.push_back( value );
}

void RealList::Resize( int new_size )
{
	data.resize( new_size );
}

IntList::IntList()
{
	;
}

IntList::~IntList()
{
	;
}

IntList::IntList( const IntList & rhs )
{
	this->data = rhs.data;
}

size_t IntList::GetNElements()
{
	return data.size();
}

void IntList::AddData( int value )
{
	data.push_back( value );
}

void IntList::Resize( int new_size )
{
	data.resize( new_size );
}

void IntList::ReOrder( IntList & orderMap )
{
	IntList dataSwap = * this;
	size_t nElems = this->GetNElements();

	for ( size_t i = 0; i < nElems; ++ i )
	{
		size_t j = orderMap[ i ];
		( * this )[ i ] = dataSwap[ j ];
	}
}

EList::EList()
{
	;
}

EList::~EList()
{
	;
}

size_t EList::GetNElements()
{
	return data.size();
}

void EList::AddElem( IntList &elem )
{
	this->data.push_back( elem.data );
}

void EList::AddElem( vector< int > &elem )
{
	this->data.push_back( elem );
}

void EList::ReOrder( IntList & orderMap )
{
	EList dataSwap = * this;
	size_t nElems = this->GetNElements();

	for ( size_t i = 0; i < nElems; ++ i )
	{
		size_t j = orderMap[ i ];
		( * this )[ i ] = dataSwap[ j ];
	}
}

void EList::Resize( int new_size )
{
	data.resize( new_size );
}

ScalarBcco::ScalarBcco()
{
}

ScalarBcco::~ScalarBcco()
{
}

void ScalarBcco::AddBcPoint( int bcVertex )
{
	vertexList.push_back( bcVertex );
}

void ScalarBcco::ScanBcFace( ScalarGrid * grid )
{
	cout << " BCTypeName = " << ONEFLOW::GetCgnsBcName( this->bcType ) << endl;

	IntSet bcVertex;
	this->ProcessVertexBc( bcVertex );
	
	grid->ScanBcFace( bcVertex, this->bcType );
}

void ScalarBcco::ProcessVertexBc( IntSet & bcVertex )
{
	for ( int iBcPoint = 0; iBcPoint < this->vertexList.size(); ++ iBcPoint )
	{
		bcVertex.insert( this->vertexList[ iBcPoint ] );
	}
}


ScalarBccos::ScalarBccos()
{
}

ScalarBccos::~ScalarBccos()
{
	for ( int i = 0; i < this->bccos.size(); ++ i )
	{
		delete this->bccos[ i ];
	}
}

void ScalarBccos::AddBcco( ScalarBcco * scalarBcco )
{
	this->bccos.push_back( scalarBcco );
}

void ScalarBccos::ScanBcFace( ScalarGrid * grid )
{
	for ( int iBoco = 0; iBoco < this->bccos.size(); ++ iBoco )
	{
		cout << " iBoco = " << iBoco << " ";
		this->bccos[ iBoco ]->ScanBcFace( grid );
	}
}

ScalarGrid::ScalarGrid()
{
	scalarBccos = new ScalarBccos();
	dataBase = new DataBase();
	this->grid_id = 0;
	this->scalarIFace = new ScalarIFace( this->grid_id );
	this->volBcType = -1;
}

ScalarGrid::ScalarGrid( int grid_id )
{
	scalarBccos = new ScalarBccos();
	dataBase = new DataBase();
	this->grid_id = grid_id;
	this->scalarIFace = new ScalarIFace( this->grid_id );
	this->volBcType = -1;
}

ScalarGrid::~ScalarGrid()
{
	delete this->scalarBccos;
	delete this->dataBase;
	delete this->scalarIFace;
}

int ScalarGrid::GetNNodes()
{
	return this->xn.GetNElements();
}

int ScalarGrid::GetNCells()
{
	return this->eTypes.GetNElements();
}

int ScalarGrid::GetNTCells()
{
	size_t nBFaces = this->GetNBFaces();
	size_t nCells = this->GetNCells();
	return nBFaces + nCells;
}

int ScalarGrid::GetNFaces()
{
	return this->faces.GetNElements();
}

int ScalarGrid::GetNBFaces()
{
	return this->bcTypes.GetNElements();
}

void ScalarGrid::GenerateGrid( int ni, Real xmin, Real xmax )
{
	Real dx = ( xmax - xmin ) / ( ni - 1 );

	for ( int i = 0; i < ni; ++ i )
	{
		Real xm = xmin + i * dx;
		Real ym = 0.0;
		Real zm = 0.0;

		xn.AddData( xm );
		yn.AddData( ym );
		zn.AddData( zm );
	}

	int ptL = 0;
	int ptR = ni - 1;

	for ( int i = 0; i < ni - 1; ++ i )
	{
		int p1 = i;
		int p2 = i + 1;

		int eType = ONEFLOW::BAR_2;

		this->PushElement( p1, p2, eType );
	}
	ScalarBcco * scalarBccoL = new ScalarBcco();
	//scalarBccoL->bcType = ONEFLOW::BCInflow;
	scalarBccoL->bcType = ONEFLOW::BCOutflow;
	scalarBccoL->AddBcPoint( ptL );
	scalarBccos->AddBcco( scalarBccoL );

	ScalarBcco * scalarBccoR = new ScalarBcco();
	scalarBccoR->bcType = ONEFLOW::BCOutflow;
	scalarBccoR->AddBcPoint( ptR );
	scalarBccos->AddBcco( scalarBccoR );

}

void ScalarGrid::PushElement( int p1, int p2, int eType )
{
	IntList elem;
	elem.AddData( p1 );
	elem.AddData( p2 );

	this->elements.AddElem( elem );
	this->eTypes.AddData( eType );
}

void ScalarGrid::AllocGeom()
{
	this->nFaces = this->GetNFaces();
	this->nCells = this->GetNCells();
	this->nBFaces = this->GetNBFaces();

	size_t nTCells = this->nBFaces + this->nCells;

	this->xfc.Resize( this->nFaces );
	this->yfc.Resize( this->nFaces );
	this->zfc.Resize( this->nFaces );

	this->xfn.Resize( this->nFaces );
	this->yfn.Resize( this->nFaces );
	this->zfn.Resize( this->nFaces );
	this->area.Resize( this->nFaces );

	this->xcc.Resize( nTCells );
	this->ycc.Resize( nTCells );
	this->zcc.Resize( nTCells );
	this->vol.Resize( nTCells );
}

void ScalarGrid::CalcMetrics1D()
{
	//must compute face center first for one dimensional case
	//then face normal
	this->AllocGeom();
	this->CalcFaceCenter1D();
	this->CalcCellCenterVol1D();
	this->CalcFaceNormal1D();
	this->CalcGhostCellCenterVol1D();
}

void ScalarGrid::CalcFaceCenter1D()
{
	this->nFaces = this->GetNFaces();
	for ( int iFace = 0; iFace < this->nFaces; ++ iFace )
	{
		vector< int > & faceNodes = this->faces[ iFace ];
		int p1 = faceNodes[ 0 ];
		int p2 = faceNodes[ 0 ];
		this->xfc[ iFace ] = half * ( this->xn[ p1 ] + this->xn[ p2 ] );
		this->yfc[ iFace ] = half * ( this->yn[ p1 ] + this->yn[ p2 ] );
		this->zfc[ iFace ] = half * ( this->zn[ p1 ] + this->zn[ p2 ] );
	}
}

//void ScalarGrid::CalcCellCenterVol1D()
//{
//	this->nCells = this->GetNCells();
//	
//	for ( size_t iCell = 0; iCell < this->nCells; ++ iCell )
//	{
//		vector< int > & element = this->elements[ iCell ];
//		int p1 = element[ 0 ];
//		int p2 = element[ 1 ];
//		this->xcc[ iCell  ] = half * ( this->xn[ p1 ] + this->xn[ p2 ] );
//		this->ycc[ iCell  ] = half * ( this->yn[ p1 ] + this->yn[ p2 ] );
//		this->zcc[ iCell  ] = half * ( this->zn[ p1 ] + this->zn[ p2 ] );
//		Real dx = this->xn[ p2 ] - this->xn[ p1 ];
//		Real dy = this->yn[ p2 ] - this->yn[ p1 ];
//		Real dz = this->zn[ p2 ] - this->zn[ p1 ];
//		this->vol[ iCell  ] = ONEFLOW::DIST( dx, dy, dz );
//	}
//}


void ScalarGrid::CalcCellCenter1D()
{
	this->xcc = 0;
	this->ycc = 0;
	this->zcc = 0;

	this->nFaces = this->GetNFaces();
	this->nBFaces = this->GetNBFaces();
	for ( int iFace = 0; iFace < this->nBFaces; ++ iFace )
	{
		vector< int > & faceNodes = this->faces[ iFace ];
		int lc  = this->lc[ iFace ];

		int pt = faceNodes[ 0 ];

		this->xcc[ lc ] += this->xn[ pt ];
		this->ycc[ lc ] += this->yn[ pt ];
		this->zcc[ lc ] += this->zn[ pt ];
	}

	for ( int iFace = nBFaces; iFace < this->nFaces; ++ iFace )
	{
		vector< int > & faceNodes = this->faces[ iFace ];
		int lc  = this->lc[ iFace ];
		int rc  = this->rc[ iFace ];

		int pt = faceNodes[ 0 ];

		this->xcc[ lc ] += this->xn[ pt ];
		this->ycc[ lc ] += this->yn[ pt ];
		this->zcc[ lc ] += this->zn[ pt ];

		this->xcc[ rc ] += this->xn[ pt ];
		this->ycc[ rc ] += this->yn[ pt ];
		this->zcc[ rc ] += this->zn[ pt ];
	}

	this->nCells = this->GetNCells();

	for ( size_t iCell = 0; iCell < this->nCells; ++ iCell )
	{
		this->xcc[ iCell  ] *= half;
		this->ycc[ iCell  ] *= half;
		this->zcc[ iCell  ] *= half;
	}
}

void ScalarGrid::CalcCellVolume1D()
{
	this->vol = 0;

	this->nFaces = this->GetNFaces();
	for ( int iFace = 0; iFace < this->nFaces; ++ iFace )
	{
		vector< int > & faceNodes = this->faces[ iFace ];
		int lc  = this->lc[ iFace ];
		int rc  = this->rc[ iFace ];

		int pt = faceNodes[ 0 ];

		Real dxl = this->xfc[ iFace ] - this->xcc[ lc ];
		Real dyl = this->yfc[ iFace ] - this->ycc[ lc ];
		Real dzl = this->zfc[ iFace ] - this->zcc[ lc ];

		Real dxr = this->xfc[ iFace ] - this->xcc[ rc ];
		Real dyr = this->yfc[ iFace ] - this->ycc[ rc ];
		Real dzr = this->zfc[ iFace ] - this->zcc[ rc ];

		Real dsl = ONEFLOW::DIST( dxl, dyl, dzl );
		Real dsr = ONEFLOW::DIST( dxr, dyr, dzr );

		this->vol[ lc ] += dsl;
		this->vol[ rc ] += dsr;
	}
}

void ScalarGrid::CalcCellCenterVol1D()
{
	this->CalcCellCenter1D();
	this->CalcCellVolume1D();
}

void ScalarGrid::CalcFaceNormal1D()
{
	this->nFaces = this->GetNFaces();
	for ( size_t iFace = 0; iFace < this->nFaces; ++ iFace )
	{
		int lc  = this->lc[ iFace ];

		Real dx = this->xfc[ iFace ] - this->xcc[ lc ];
		Real dy = this->yfc[ iFace ] - this->ycc[ lc ];
		Real dz = this->zfc[ iFace ] - this->zcc[ lc ];
		Real ds = ONEFLOW::DIST( dx, dy, dz );

		Real factor   = 1.0 / ( ds + SMALL );
		this->xfn[ iFace ] = factor * dx;
		this->yfn[ iFace ] = factor * dy;
		this->zfn[ iFace ] = factor * dz;

		this->area[ iFace ] = 1.0;
	}
}

void ScalarGrid::CalcGhostCellCenterVol1D()
{
	this->nBFaces = this->GetNBFaces();
	this->nCells = this->GetNCells();

	// For ghost cells
	for ( size_t iFace = 0; iFace < nBFaces; ++ iFace )
	{
		int lc = this->lc[ iFace ];
		int rc = this->rc[ iFace ];
		if ( this->area[ iFace ] > SMALL )
		{
			Real tmp = 2.0 * ( ( this->xcc[ lc ] - this->xfc[ iFace ] ) * this->xfn[ iFace ]
				             + ( this->ycc[ lc ] - this->yfc[ iFace ] ) * this->yfn[ iFace ]
				             + ( this->zcc[ lc ] - this->zfc[ iFace ] ) * this->zfn[ iFace ] );
			this->xcc[ rc ] = this->xcc[ lc ] - this->xfn[ iFace ] * tmp;
			this->ycc[ rc ] = this->ycc[ lc ] - this->yfn[ iFace ] * tmp;
			this->zcc[ rc ] = this->zcc[ lc ] - this->zfn[ iFace ] * tmp;
		}
		else
		{
			// Degenerated faces
			this->xcc[ rc ] = - this->xcc[ lc ] + 2.0 * this->xfc[ iFace ];
			this->ycc[ rc ] = - this->ycc[ lc ] + 2.0 * this->yfc[ iFace ];
			this->zcc[ rc ] = - this->zcc[ lc ] + 2.0 * this->zfc[ iFace ];
		}
		this->vol[ rc ] = this->vol[ lc ];
	}
}

void ScalarGrid::CalcTopology()
{
	this->nNodes = this->GetNNodes();

	this->nCells = this->GetNCells();

	set< HXSort< IntField > > faceSet;
	HXSort< IntField > faceForSorting;

	for ( int iCell = 0; iCell < nCells; ++ iCell )
	{
		vector< int > & element = elements[ iCell ];

		int eType = eTypes[ iCell ];

		UnitElement * unitElement = ElementHome::GetUnitElement( eType );

		int numberOfFaceInElement = unitElement->GetElementFaceNumber();

		for ( int iLocalFace = 0; iLocalFace < numberOfFaceInElement; ++ iLocalFace )
		{
			IntField & localFaceNodeIndexArray = unitElement->GetElementFace( iLocalFace );
			int faceType = unitElement->GetFaceType( iLocalFace );
			int numberOfFacePoints = localFaceNodeIndexArray.size();
			IntList faceNodeIndexArray;
			for ( int iFacePoint = 0; iFacePoint < numberOfFacePoints; ++ iFacePoint )
			{
				faceNodeIndexArray.AddData( element[ localFaceNodeIndexArray[ iFacePoint ] ] );
			}

			IntField faceNodeIndexArraySort = faceNodeIndexArray.data;
			std::sort( faceNodeIndexArraySort.begin(), faceNodeIndexArraySort.end() );
			faceForSorting.value = faceNodeIndexArraySort;

			set< HXSort< IntField > >::iterator iter = faceSet.find( faceForSorting );
			if ( iter == faceSet.end() )
			{
				faceForSorting.index = faceSet.size();
				faceSet.insert( faceForSorting );
				int faceIndex = faceForSorting.index;
				int newSize = faceIndex + 1;
				this->lc.Resize( newSize );
				this->rc.Resize( newSize );
				this->lpos.Resize( newSize );
				this->rpos.Resize( newSize );
				this->fTypes.Resize( newSize );
				this->fBcTypes.Resize( newSize );

				this->fTypes[ faceIndex ] = faceType;
				this->fBcTypes[ faceIndex ] = ONEFLOW::INVALID_INDEX;
				this->lc[ faceIndex ] = iCell;
				this->rc[ faceIndex ] = ONEFLOW::INVALID_INDEX;
				this->lpos[ faceIndex ] = iLocalFace;
				this->rpos[ faceIndex ] = ONEFLOW::INVALID_INDEX;
				this->faces.AddElem( faceNodeIndexArray );
			}
			else
			{
				int faceIndex = iter->index;
				this->fBcTypes[ faceIndex ] = ONEFLOW::BCTypeNull; //inner bc
				this->rc[ faceIndex ] = iCell;
				this->rpos[ faceIndex ] = iLocalFace;
			}
		}
	}

	this->ReorderFaces();
	this->ScanBcFace();
	this->SetBcGhostCell();
}


void ScalarGrid::ScanBcFace()
{
	this->AllocateBc();
	scalarBccos->ScanBcFace( this );
	this->SetBcTypes();
}

void ScalarGrid::CalcOrderMap( IntList &orderMap )
{
	this->nFaces = this->faces.GetNElements();
	orderMap.Resize( this->nFaces );

	int iBoundaryFaceCount = 0;
	int iCount = 0;
	for ( int iFace = 0; iFace < this->nFaces; ++ iFace )
	{
		int rc = this->rc[ iFace ];
		if ( rc == ONEFLOW::INVALID_INDEX )
		{
			orderMap[ iCount ++ ] = iFace;
			++ iBoundaryFaceCount;
		}
	}

	this->nBFaces = iBoundaryFaceCount;

	for ( int iFace = 0; iFace < this->nFaces; ++ iFace )
	{
		int rc = this->rc[ iFace ];
		if ( rc != ONEFLOW::INVALID_INDEX )
		{
			orderMap[ iCount ++ ] = iFace;
		}
	}

}

void ScalarGrid::ReorderFaces()
{
	IntList orderMap;
	this->CalcOrderMap( orderMap );

	this->lc.ReOrder( orderMap );
	this->rc.ReOrder( orderMap );

	this->lpos.ReOrder( orderMap );
	this->rpos.ReOrder( orderMap );

	this->fTypes.ReOrder( orderMap );
	this->fBcTypes.ReOrder( orderMap );
	this->faces.ReOrder( orderMap );
}

void ScalarGrid::SetBcGhostCell()
{
	this->nCells = this->GetNCells();
	this->nBFaces = this->GetNBFaces();

	for ( int iFace = 0; iFace < this->nBFaces; ++ iFace )
	{
		this->rc[ iFace ] = iFace + nCells;
	}
}

bool ScalarGrid::CheckBcFace( IntSet & bcVertex, vector< int > & nodeId )
{
	int size = nodeId.size();
	for ( int iNode = 0; iNode < size; ++ iNode )
	{
		IntSet::iterator iter = bcVertex.find( nodeId[ iNode ] );
		if ( iter == bcVertex.end() )
		{
			return false;
		}
	}
	return true;
}

void ScalarGrid::AllocateBc()
{
	this->nFaces = this->faces.GetNElements();
	cout << " nFaces = " << nFaces << "\n";

	int nTraditionalBc = 0;
	for ( int iFace = 0; iFace < nFaces; ++ iFace )
	{
		int originalBcType = this->fBcTypes[ iFace ];
		if ( originalBcType == ONEFLOW::INVALID_INDEX )
		{
			++ nTraditionalBc;
		}
	}
	cout << " nTraditionalBc = " << nTraditionalBc << "\n";
	this->bcTypes.Resize( nTraditionalBc );
}

void ScalarGrid::ScanBcFace( IntSet& bcVertex, int bcType )
{
	this->nFaces = this->faces.GetNElements();
	int nBcFaces_local = 0;
	for ( int iFace = 0; iFace < nFaces; ++ iFace )
	{
		int originalBcType = this->fBcTypes[ iFace ];

		if ( originalBcType == ONEFLOW::INVALID_INDEX )
		{
			if ( this->CheckBcFace( bcVertex, this->faces[ iFace ] ) )
			{
				++ nBcFaces_local;

				this->fBcTypes[ iFace ] = bcType;
			}
		}
	}

	cout << " nFinalBcFace = " << nBcFaces_local << " bcType = " << bcType << endl;
	int kkk = 1;
}

void ScalarGrid::SetBcTypes()
{
	this->nBFaces = this->GetNBFaces();
	for ( int iFace = 0; iFace < nBFaces; ++ iFace )
	{
		int bcType = this->fBcTypes[ iFace ];
		this->bcTypes[ iFace ] = bcType;
	}
}

void ScalarGrid::CalcC2C( EList & c2c )
{
	if ( c2c.GetNElements() != 0 ) return;

	this->nFaces = this->GetNFaces();
	this->nCells = this->GetNCells();
	this->nBFaces = this->GetNBFaces();

	c2c.Resize( nCells );

	// If boundary is an INTERFACE, need to count ghost cell
	for ( int iFace = 0; iFace < nBFaces; ++ iFace )
	{
		int bcType = this->bcTypes[ iFace ];
		if ( BC::IsInterfaceBc( bcType ) )
		{
			int lc  = this->lc[ iFace ];
			int rc  = this->rc[ iFace ];
			c2c[ lc  ].push_back( rc );
		}
	}

	for ( int iFace = nBFaces; iFace < nFaces; ++ iFace )
	{
		int lc  = this->lc[ iFace ];
		int rc  = this->rc[ iFace ];
		c2c[ lc ].push_back( rc );
		c2c[ rc ].push_back( lc );
	}
}

void ScalarGrid::CalcInterfaceToBcFace()
{
	if ( this->scalarIFace->GetNIFaces() == 0 ) return;
	int nBFaces = this->GetNBFaces();

	this->scalarIFace->interface_to_bcface.resize( 0 );

	for ( int iBFace = 0; iBFace < nBFaces; ++ iBFace )
	{
		if ( ! BC::IsInterfaceBc( this->bcTypes[ iBFace ] ) )
		{
			continue;
		}

		this->scalarIFace->interface_to_bcface.push_back( iBFace );
	}
}

void ScalarGrid::Normalize()
{
	int nFaces = this->faces.GetNElements();
	for ( int iFace = 0; iFace < nFaces; ++ iFace )
	{
		if ( this->lc[ iFace ] < 0 )
		{
			//need to reverse the node ordering
			vector< int > & face = this->faces[ iFace ];
			std::reverse( face.begin(), face.end() );
			// now reverse lc and rc
			ONEFLOW::SWAP( this->lc[ iFace ], this->rc[ iFace ] );
		}
	}

	this->SetBcGhostCell();
}

void ScalarGrid::GetSId( int i_interface, int & sId )
{
	int iBFace = this->scalarIFace->interface_to_bcface[ i_interface ];
	sId = this->lc[ iBFace ];
}

void ScalarGrid::GetTId( int i_interface, int & tId )
{
	int iBFace = this->scalarIFace->interface_to_bcface[ i_interface ];
	tId = this->rc[ iBFace ];
}

void ScalarGrid::DumpCalcGrid()
{
	cout << "Dumping unstructured grid data files......\n";
	fstream file;
	string fileName = "scalar.ofl";
	OpenPrjFile( file, fileName, ios_base::out | ios_base::binary );
	DataBook * databook = new DataBook();
	this->WriteGrid( databook );
	databook->WriteFile( file );
	delete databook;
	CloseFile( file );
}

void ScalarGrid::WriteGrid( DataBook * databook )
{
	ONEFLOW::HXWrite( databook, this->nNodes );
	ONEFLOW::HXWrite( databook, this->nCells );
	ONEFLOW::HXWrite( databook, this->nFaces );

	//node
	ONEFLOW::HXWrite( databook, this->xn.data );
	ONEFLOW::HXWrite( databook, this->yn.data );
	ONEFLOW::HXWrite( databook, this->zn.data );

	//element
	ONEFLOW::HXWrite( databook, this->eTypes.data );

	ONEFLOW::HXWrite( databook, this->volBcType  );

	this->WriteGridFaceTopology( databook );
	this->WriteBoundaryTopology( databook );
}

void ScalarGrid::ReadCalcGrid()
{
	fstream file;
	string fileName = "scalar.ofl";
	OpenPrjFile( file, fileName, ios_base::in | ios_base::binary );
	DataBook * databook = new DataBook();
	databook->ReadFile( file );
	this->ReadGrid( databook );
	delete databook;
	CloseFile( file );
}

void ScalarGrid::ReadGrid( DataBook * databook )
{
	cout << "Reading unstructured grid data files......\n";
	//Read the number of nodes, number of elements and number of elements faces

	ONEFLOW::HXRead( databook, this->nNodes );
	ONEFLOW::HXRead( databook, this->nCells );
	ONEFLOW::HXRead( databook, this->nFaces );

	cout << "Grid dimension = " << Dim::dimension << endl;

	cout << " number of nodes    : " << this->nNodes << endl;
	cout << " number of elements : " << this->nCells << endl;
	cout << " number of faces    : " << this->nFaces << endl;

	this->CreateNodes( this->nNodes );

	ONEFLOW::HXRead( databook, this->xn.data );
	ONEFLOW::HXRead( databook, this->yn.data );
	ONEFLOW::HXRead( databook, this->zn.data );

	//element
	this->eTypes.Resize( this->nCells );
	ONEFLOW::HXRead( databook, this->eTypes.data );

	cout << "The grid nodes have been read\n";
	ONEFLOW::HXRead( databook, this->volBcType  );

	//this->nodeMesh->CalcMinMaxBox();
	this->ReadGridFaceTopology( databook );
	this->ReadBoundaryTopology( databook );
	//this->NormalizeBc();

	cout << "All the computing information is ready!\n";
}

void ScalarGrid::CreateNodes( int numberOfNodes )
{
	this->xn.Resize( numberOfNodes );
	this->yn.Resize( numberOfNodes );
	this->zn.Resize( numberOfNodes );
}

void ScalarGrid::WriteGridFaceTopology( DataBook * databook )
{
	ONEFLOW::HXWrite( databook, this->fTypes.data );

	IntField numFaceNode( this->nFaces );

	for ( int iFace = 0; iFace < this->nFaces; ++ iFace )
	{
		numFaceNode[ iFace ] = this->faces[ iFace ].size();
	}

	ONEFLOW::HXWrite( databook, numFaceNode );

	IntField faceNodeMem;

	for ( int iFace = 0; iFace < this->nFaces; ++ iFace )
	{
		int nNode = numFaceNode[ iFace ];
		for ( int iNode = 0; iNode < nNode; ++ iNode )
		{
			faceNodeMem.push_back( this->faces[ iFace ][ iNode ] );
		}
	}
	ONEFLOW::HXWrite( databook, faceNodeMem );

	ONEFLOW::HXWrite( databook, this->lc.data );
	ONEFLOW::HXWrite( databook, this->rc.data );
}

void ScalarGrid::ReadGridFaceTopology( DataBook * databook )
{
	this->faces.Resize( this->nFaces );
	this->lc.Resize( this->nFaces );
	this->rc.Resize( this->nFaces );
	this->fTypes.Resize( this->nFaces );

	ONEFLOW::HXRead( databook, this->fTypes.data );

	IntField numFaceNode( this->nFaces );

	ONEFLOW::HXRead( databook, numFaceNode );

	int nsum = ONEFLOW::SUM( numFaceNode );

	cout << "Setting the connection mode of face to point......\n";
	IntField faceNodeMem( nsum );

	ONEFLOW::HXRead( databook, faceNodeMem );

	int ipos = 0;
	for ( int iFace = 0; iFace < this->nFaces; ++ iFace )
	{
		int nNode = numFaceNode[ iFace ];
		for ( int iNode = 0; iNode < nNode; ++ iNode )
		{
			int pid = faceNodeMem[ ipos ++ ];
			this->faces[ iFace ].push_back( pid );
		}
	}

	cout << "Setting the connection mode of face to cell......\n";

	ONEFLOW::HXRead( databook, this->lc.data );
	ONEFLOW::HXRead( databook, this->rc.data );

	for ( int iFace = 0; iFace < this->nFaces; ++ iFace )
	{
		if ( this->lc[ iFace ] < 0 )
		{
			//need to reverse the node ordering
			vector< int > & face = this->faces[ iFace ];
			std::reverse( face.begin(), face.end() );
			// now reverse leftCellIndex  and rightCellIndex
			ONEFLOW::SWAP( this->lc[ iFace ], this->rc[ iFace ] );
		}
	}
}

void ScalarGrid::WriteBoundaryTopology( DataBook * databook )
{
	int nBFaces = this->GetNBFaces();
	ONEFLOW::HXWrite( databook, nBFaces );

	ONEFLOW::HXWrite( databook, this->bcTypes.data );
	this->bcNameIds = this->bcTypes;
	ONEFLOW::HXWrite( databook, this->bcNameIds.data );

	this->scalarIFace->WriteInterfaceTopology( databook );
}

void ScalarGrid::ReadBoundaryTopology( DataBook * databook )
{
	cout << "Setting the boundary condition......\n";
	ONEFLOW::HXRead( databook, this->nBFaces );

	this->bcTypes.Resize( this->nBFaces );
	this->bcNameIds.Resize( this->nBFaces );

	//Setting boundary conditions
	ONEFLOW::HXRead( databook, this->bcTypes.data );
	ONEFLOW::HXRead( databook, this->bcNameIds.data );

	this->scalarIFace->ReadInterfaceTopology( databook );
}

//for partition
void ScalarGrid::AddPhysicalBcFace( int global_face_id, int bctype, int lcell, int rcell )
{
	this->global_faceid.push_back( global_face_id );
	this->bcTypes.AddData( bctype );
	this->fBcTypes.AddData( bctype );

	this->lc.AddData( lcell );
	this->rc.AddData( rcell );
}

void ScalarGrid::AddInterfaceBcFace( int global_face_id, int bctype, int lcell, int rcell, int nei_zoneid, int nei_cellid )
{
	this->global_faceid.push_back( global_face_id );
	this->bcTypes.AddData( bctype );
	this->fBcTypes.AddData( bctype );

	this->lc.AddData( lcell );
	this->rc.AddData( rcell );

	this->AddInterface( global_face_id, nei_zoneid, nei_cellid );
}

void ScalarGrid::AddInnerFace( int global_face_id, int bctype, int lcell, int rcell )
{
	this->global_faceid.push_back( global_face_id );
	this->fBcTypes.AddData( bctype );

	this->lc.AddData( lcell );
	this->rc.AddData( rcell );
}

void ScalarGrid::AddInterface( int global_interface_id, int neighbor_zoneid, int neighbor_cellid )
{
	this->scalarIFace->AddInterface( global_interface_id, neighbor_zoneid, neighbor_cellid );
}

void ScalarGrid::ReconstructNode( ScalarGrid * ggrid )
{
	int nFaces = this->global_faceid.size();
	set<int> nodeset;

	for ( int iFace = 0; iFace < nFaces; ++ iFace )
	{
		//global face id
		int iGFace = this->global_faceid[ iFace ];
		vector< int > & face = ggrid->faces[ iGFace ];
		int nNode = face.size();
		for ( int iNode = 0; iNode < nNode; ++ iNode )
		{
			nodeset.insert( face[ iNode ] );
		}
		this->faces.AddElem( face );
	}

	map<int, int> global_local_node;
	int count = 0;
	for ( set<int>::iterator iter = nodeset.begin(); iter != nodeset.end(); ++ iter )
	{
		global_local_node.insert( pair<int, int>( *iter, count ++ ) );
	}

	for ( int iFace = 0; iFace < nFaces; ++ iFace )
	{
		vector< int > & face = this->faces[ iFace ];
		int nNode = face.size();
		for ( int iNode = 0; iNode < nNode; ++ iNode )
		{
			int glbal_node_id = face[ iNode ];
			face[ iNode ] = global_local_node[ glbal_node_id ];
		}
	}

	for ( set<int>::iterator iter = nodeset.begin(); iter != nodeset.end(); ++ iter )
	{
		int iNode = *iter;
		Real xm = ggrid->xn[ iNode ];
		Real ym = ggrid->yn[ iNode ];
		Real zm = ggrid->zn[ iNode ];

		this->xn.AddData( xm );
		this->yn.AddData( ym );
		this->zn.AddData( zm );
	}
}




EndNameSpace