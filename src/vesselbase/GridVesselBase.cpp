/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2012 The plumed team
   (see the PEOPLE file at the root of the distribution for a list of names)

   See http://www.plumed-code.org for more information.

   This file is part of plumed, version 2.0.

   plumed is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
    
   plumed is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with plumed.  If not, see <http://www.gnu.org/licenses/>.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
#include "GridVesselBase.h"
#include "ActionWithVessel.h"
#include "tools/Tools.h"

namespace PLMD {
namespace vesselbase {

void GridVesselBase::registerKeywords( Keywords& keys ){
  keys.add("compulsory","MIN","minimum values for the grid");
  keys.add("compulsory","MAX","maximum values for the grid");
  keys.add("compulsory","NBIN","number of bins in each direction for the grid");
}

GridVesselBase::GridVesselBase( const VesselOptions& da ):
Vessel(da),
nper(0)
{
  if( getName().find("GRID")==std::string::npos ){
     plumed_merror("grid vessels must have the word GRID in their keyword");
  }
  parseVector("MIN",str_min); dimension=str_min.size();
  str_max.resize( dimension ); nbin.resize( dimension );
  parseVector("MAX",str_max); parseVector("NBIN",nbin);
  tmp_indices.resize( str_min.size() );
}

void GridVesselBase::finishSetup( const unsigned& nelem, const std::vector<std::string>& names ){
  nper=nelem; dimension=str_min.size();
  plumed_massert( names.size()==nper+dimension, "number of field names does not match number of elements per node"); 

  npoints=1; dx.resize( dimension ); min.resize( dimension ); 
  max.resize( dimension ); pbc.resize( dimension ); arg_names.resize( nper + dimension );
  for(unsigned i=0;i<dimension;++i){
      Tools::convert( str_min[i], min[i] ); 
      Tools::convert( str_max[i], max[i] );
      dx[i] = ( max[i] - min[i] ) / static_cast<double>( nbin[i] );
      max[i] +=dx[i]; nbin[i]+=1; pbc[i]=false; 
      npoints*=nbin[i]; 
  }   
  for(unsigned i=0;i<(nper+dimension);++i) arg_names[i]=names[i];
}

void GridVesselBase::finishSetup( const std::vector<Value*>& arguments, const std::string& funcname, const bool usederiv ){
  plumed_massert( arguments.size()!=str_min.size(), "number of arguments does not match size of min and max arrays");

  dimension=str_min.size();
  if( usederiv ) nper=1+arguments.size();
  else nper=1;

  npoints=1; dx.resize( dimension ); min.resize( dimension ); 
  max.resize( dimension ); pbc.resize( dimension ); arg_names.resize( nper + dimension );
  for(unsigned i=0;i<dimension;++i){
      arg_names[i]=arguments[i]->getName();
      if( arguments[i]->isPeriodic() ){
          pbc[i]=true;
          arguments[i]->getDomain( str_min[i], str_max[i] );
      }  else {
          pbc[i]=false;
      }
      Tools::convert( str_min[i], min[i] ); 
      Tools::convert( str_max[i], max[i] );
      dx[i] = ( max[i] - min[i] ) / static_cast<double>( nbin[i] );
      if( !pbc[i] ){ max[i] += dx[i]; nbin[i] +=1; }
      npoints*=nbin[i];
  }
  arg_names[dimension]=funcname;
  if( usederiv ){
      for(unsigned i=0;i<dimension;++i) arg_names[dimension+i]="der_"+arguments[i]->getName();
  }
}

std::string GridVesselBase::getGridDescription() const {
  std::string des="grid of "; std::string num;
  for(unsigned i=0;i<dimension-1;++i){
      Tools::convert( nbin[i], num );
      des += num + " X ";
  }
  Tools::convert( nbin[dimension-1], num );
  des += num + " equally spaced points between (";
  for(unsigned i=0;i<dimension-1;++i) des += str_min[i] + ",";
  Tools::convert( nbin[dimension-1], num );
  des += str_min[dimension-1] + ") and (";
  for(unsigned i=0;i<dimension-1;++i) des += str_max[i] + ",";
  des += str_max[dimension-1] + ")";
  return des;
}

void GridVesselBase::resize(){
  plumed_massert( nper>0, "Number of datapoints at each grid point has not been set");
  resizeBuffer( npoints*nper );
}

unsigned GridVesselBase::getIndex( const std::vector<unsigned>& indices ) const {
  plumed_dbg_assert( indices.size()==dimension );
  // indices are flattended using a column-major order
  unsigned index=indices[dimension-1];
  for(unsigned i=dimension-1;i>0;--i){
    index=index*nbin[i-1]+indices[i-1];
  } 
  return index;
}

void GridVesselBase::getIndices( const unsigned& index, std::vector<unsigned>& indices ) const {
 unsigned kk=index;
 indices[0]=index%nbin[0];
 for(unsigned i=1;i<dimension-1;++i){
    kk=(kk-indices[i-1])/nbin[i-1];
    indices[i]=kk%nbin[i];
 } 
 if(dimension>=2){  // I think this is wrong
    indices[dimension-1]=(kk-indices[dimension-2])/nbin[dimension-2];
 }
}

void GridVesselBase::getGridPointCoordinates( const unsigned& ipoint , std::vector<double>& x ){
  plumed_dbg_assert( x.size()==dimension && ipoint<npoints );
  currentGridPoint=ipoint; getIndices( ipoint, tmp_indices ); 
  for(unsigned i=0;i<dimension;++i) x[i] = min[i] + dx[i]*tmp_indices[i];
}

void GridVesselBase::getIndices(const std::vector<double>& x, std::vector<unsigned>& indices) const {
  plumed_dbg_assert(x.size()==dimension && indices.size()==dimension);
  for(unsigned int i=0;i<dimension;++i){
    indices[i]=static_cast<unsigned>(std::floor((x[i]-min[i])/dx[i]));
  }
}

double GridVesselBase::getGridElement( const unsigned& ipoint, const unsigned& jelement ) const {
  plumed_dbg_assert( ipoint<npoints && jelement<nper );
  return getBufferElement( nper*ipoint + jelement );
}

void GridVesselBase::setGridElement( const unsigned& ipoint, const unsigned& jelement, const double& value ){
  plumed_dbg_assert( ipoint<npoints && jelement<nper );
  setBufferElement( nper*ipoint + jelement, value );
}

void GridVesselBase::addToGridElement( const unsigned& ipoint, const unsigned& jelement, const double& value ){
  plumed_dbg_assert( ipoint<npoints && jelement<nper );
  addToBufferElement( nper*ipoint + jelement, value );
}

double GridVesselBase::getGridElement( const std::vector<unsigned>& indices, const unsigned& jelement ) const {
  return getGridElement( getIndex( indices ), jelement );
}

void GridVesselBase::setGridElement( const std::vector<unsigned>& indices, const unsigned& jelement, const double& value ){
  setGridElement( getIndex( indices ), jelement, value );
}

void GridVesselBase::addToGridElement( const std::vector<unsigned>& indices, const unsigned& jelement, const double& value ){
  addToGridElement( getIndex( indices ), jelement, value );
}

void GridVesselBase::writeToFile( OFile& ofile, const std::string& fmt ){
 for(unsigned i=0;i<dimension;++i){
    ofile.addConstantField("min_" + arg_names[i]);
    ofile.addConstantField("max_" + arg_names[i]);
    ofile.addConstantField("nbins_" + arg_names[i]);
    ofile.addConstantField("periodic_" + arg_names[i]);
 }

 std::vector<double> xx(dimension);
 std::vector<unsigned> indices(dimension);
 for(unsigned i=0;i<npoints;++i){
   getGridPointCoordinates(i,xx); getIndices(i,indices);
   if(i>0 && dimension>1 && indices[dimension-2]==0) ofile.printf("\n");

   for(unsigned j=0;j<dimension;++j){
      ofile.printField("min_" + arg_names[j], str_min[j] );
      ofile.printField("max_" + arg_names[j], str_max[j] );
      ofile.printField("nbins_" + arg_names[j], static_cast<int>(nbin[j]) );
      if( pbc[j] ) ofile.printField("periodic_" + arg_names[j], "true" );
      else         ofile.printField("periodic_" + arg_names[j], "false" );
   }
   for(unsigned j=0;j<dimension;++j){ ofile.fmtField(" "+fmt); ofile.printField(arg_names[j],xx[j]); }
   for(unsigned j=0;j<nper;++j){ ofile.fmtField(" "+fmt); ofile.printField( arg_names[dimension + j], getGridElement(i,j) ); }
   ofile.printField();
 }
}

}
}

