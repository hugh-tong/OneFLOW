/*---------------------------------------------------------------------------*\
    OneFLOW - LargeScale Multiphysics Scientific Simulation Environment
    Copyright (C) 2017-2023 He Xin and the OneFLOW contributors.
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

#include "UINsGrad.h"
#include "UCom.h"
#include "INsCom.h"
#include "DataBase.h"
#include "FieldImp.h"
#include "FaceTopo.h"
#include "BcRecord.h"
#include "UnsGrid.h"
#include "Zone.h"
#include "FaceMesh.h"
#include "CellMesh.h"
#include "CellTopo.h"

BeginNameSpace( ONEFLOW )

UINsGrad uins_grad;
UITGrad uit_grad;


UINsGrad::UINsGrad()
{
    ;
}

UINsGrad::~UINsGrad()
{
    ;
}

void UINsGrad::Init()
{
    UnsGrid * grid = Zone::GetUnsGrid();

    name  = "q";
    namex = "dqdx";
    namey = "dqdy";
    namez = "dqdz";

    q    = GetFieldPointer< MRField > ( grid, name  );
    dqdx = GetFieldPointer< MRField > ( grid, namex );
    dqdy = GetFieldPointer< MRField > ( grid, namey );
    dqdz = GetFieldPointer< MRField > ( grid, namez );
    bdqdx = GetFieldPointer< MRField > ( grid, "bcdqdx" );
    bdqdy = GetFieldPointer< MRField > ( grid, "bcdqdy" );
    bdqdz = GetFieldPointer< MRField > ( grid, "bcdqdz" );

    this->nEqu = nscom.nTEqu;

    this->istore = 1;
}

UITGrad::UITGrad()
{
	;
}

UITGrad::~UITGrad()
{
	;
}

void UITGrad::Init()
{
	UnsGrid * grid = Zone::GetUnsGrid();

	name = "tempr";
	namex = "dtdx";
	namey = "dtdy";
	namez = "dtdz";

	q = GetFieldPointer< MRField >(grid, name);
	dqdx = GetFieldPointer< MRField >(grid, namex);
	dqdy = GetFieldPointer< MRField >(grid, namey);
	dqdz = GetFieldPointer< MRField >(grid, namez);

	this->nEqu = nscom.nTModel;

	this->istore = 0;
}





EndNameSpace
