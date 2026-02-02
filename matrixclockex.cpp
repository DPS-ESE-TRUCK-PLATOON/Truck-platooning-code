#include <iostream>
#include <stdio.h>
#include <cmath>
#include <vector>
#include <array>
#include <sstream>
#include <string>
#include <unistd.h>
#include <algorithm>
#include <CL/cl.h>
#include "hostopencl.cpp"

using namespace std;

int main(){
    const int n = 3;
    const int pos = 2;
    
   vector<int> m_received = {
        2, 1, 0, 
        0, 0, 0,
        0, 0, 3};
    vector<int> m_current = {
        0, 0, 0, 
        0, 0, 0, 
        0, 0, 0};
    vector<int> m_updated(n*n);

    matrixclockupdate(m_received, m_current, m_updated,n,pos);
        int c=0;
    // print new matrix
    cout << "Matrix updated: \n";
    for (int k : m_updated){
        if(c==n){
            cout << endl;
            c=0;
        }
        cout << k << " ";
        c++;
    }
}
