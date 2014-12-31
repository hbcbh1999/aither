#ifndef BOUNDARYCONDITIONSHEADERDEF             //only if the macro BOUNDARYCONDITIONSHEADERDEF is not defined execute these lines of code
#define BOUNDARYCONDITIONSHEADERDEF             //define the macro

/* This header contains the class boundaryConditions.

This class stores the information needed to specify the boundary conditions for one block. */

#include <vector>  //vector
#include <string>  //string
#include <iostream> //ostream
#include "plot3d.h" //plot3dBlock

using std::ostream;
using std::vector;
using std::string;
using std::cout;
using std::endl;
using std::pair;

class boundaryConditions {
  vector<string> bcTypes;                 //vector of boundary condition names for each surface
  vector<int> iMin;                       //vector of min i coordinate defining surface
  vector<int> iMax;                       //vector of max i coordinate defining surface
  vector<int> jMin;                       //vector of min j coordinate defining surface
  vector<int> jMax;                       //vector of max j coordinate defining surface
  vector<int> kMin;                       //vector of min k coordinate defining surface
  vector<int> kMax;                       //vector of max k coordinate defining surface
  vector<int> tag;                        //vector of boundary condition tags
  int numSurfI;                           //number of i-surfaces to define boundary on in block
  int numSurfJ;                           //number of j-surfaces to define boundary on in block
  int numSurfK;                           //number of k-surfaces to define boundary on in block

 public:
  //constructor
  boundaryConditions();
  boundaryConditions( int, int, int );

  //member functions
  void SetNumSurfI( const int &a){numSurfI = a;}
  int NumSurfI()const{return numSurfI;}
  void SetNumSurfJ( const int &a){numSurfJ = a;}
  int NumSurfJ()const{return numSurfJ;}
  void SetNumSurfK( const int &a){numSurfK = a;}
  int NumSurfK()const{return numSurfK;}

  void SetBCTypes( const string &str, const int &a){bcTypes[a] = str;}
  string GetBCTypes( const int &a)const{return bcTypes[a];}
  void SetIMin( const int &i, const int &a){iMin[a] = i;}
  int GetIMin( const int &a)const{return iMin[a];}
  void SetJMin( const int &j, const int &a){jMin[a] = j;}
  int GetJMin( const int &a)const{return jMin[a];}
  void SetKMin( const int &k, const int &a){kMin[a] = k;}
  int GetKMin( const int &a)const{return kMin[a];}
  void SetIMax( const int &i, const int &a){iMax[a] = i;}
  int GetIMax( const int &a)const{return iMax[a];}
  void SetJMax( const int &j, const int &a){jMax[a] = j;}
  int GetJMax( const int &a)const{return jMax[a];}
  void SetKMax( const int &k, const int &a){kMax[a] = k;}
  int GetKMax( const int &a)const{return kMax[a];}
  void SetTag( const int &t, const int &a){tag[a] = t;}
  int GetTag( const int &a)const{return tag[a];}

  void ResizeVecs( const int &a);

  friend ostream & operator<< (ostream &os, const boundaryConditions&);

  string GetBCName(const int, const int, const int, const string&)const;

  //destructor
  ~boundaryConditions() {}

};

/* A class to store the necessary information for the interblock boundary conditions. The data is stored in pairs, where each
pair is patch on a boundary that is point matched.
*/
class interblock {

  pair<int, int> block;       //paired block numbers
  pair<int, int> boundary;    //paired boundary numbers
  pair<int, int> d1Start;     //paired first direction start numbers for surface
  pair<int, int> d1End;       //paired first direction end numbers for surface
  pair<int, int> d2Start;     //paired second direction start numbers for surface
  pair<int, int> d2End;       //paired second direction end numbers for surface
  int orientation;            //defines how patches are oriented relative to one another (1-8)

 public:
  //constructor
  interblock();

  //member functions
  void SetBlockFirst( const int &a){block.first = a;}
  int BlockFirst()const{return block.first;}
  void SetBlockSecond( const int &a){block.second = a;}
  int BlockSecond()const{return block.second;}
  void SetBlock( const pair<int,int> &a){block = a;}
  pair<int,int> Block()const{return block;}

  void SetBoundaryFirst( const int &a){boundary.first = a;}
  int BoundaryFirst()const{return boundary.first;}
  void SetBoundarySecond( const int &a){boundary.second = a;}
  int BoundarySecond()const{return boundary.second;}
  void SetBoundary( const pair<int,int> &a){boundary = a;}
  pair<int,int> Boundary()const{return boundary;}

  void SetDir1StartFirst( const int &a){d1Start.first = a;}
  int Dir1StartFirst()const{return d1Start.first;}
  void SetDir1StartSecond( const int &a){d1Start.second = a;}
  int Dir1StartSecond()const{return d1Start.second;}
  void SetDir1Start( const pair<int,int> &a){d1Start = a;}
  pair<int,int> Dir1Start()const{return d1Start;}

  void SetDir1EndFirst( const int &a){d1End.first = a;}
  int Dir1EndFirst()const{return d1End.first;}
  void SetDir1EndSecond( const int &a){d1End.second = a;}
  int Dir1EndSecond()const{return d1End.second;}
  void SetDir1End( const pair<int,int> &a){d1End = a;}
  pair<int,int> Dir1End()const{return d1End;}

  void SetDir2StartFirst( const int &a){d2Start.first = a;}
  int Dir2StartFirst()const{return d2Start.first;}
  void SetDir2StartSecond( const int &a){d2Start.second = a;}
  int Dir2StartSecond()const{return d2Start.second;}
  void SetDir2Start( const pair<int,int> &a){d2Start = a;}
  pair<int,int> Dir2Start()const{return d2Start;}

  void SetDir2EndFirst( const int &a){d2End.first = a;}
  int Dir2EndFirst()const{return d2End.first;}
  void SetDir2EndSecond( const int &a){d2End.second = a;}
  int Dir2EndSecond()const{return d2End.second;}
  void SetDir2End( const pair<int,int> &a){d2End = a;}
  pair<int,int> Dir2End()const{return d2End;}

  void SetOrientation( const int &a){orientation = a;}
  int Orientation()const{return orientation;}

  friend ostream & operator<< (ostream &os, const interblock&);

  //destructor
  ~interblock() {}

};

//function declarations
vector<interblock> GetInterblockBCs( const vector<boundaryConditions>&, const vector<plot3dBlock>& );

#endif
