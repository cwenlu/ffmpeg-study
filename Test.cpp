//
// Created by Administrator on 2017/6/8 0008.
//

#include <iostream>
#include <stdio.h>
#include "SplitVideo.h"
using std::cout;
using std::endl;

int main(int argc, char * argv[])
{
    string inputfile = "C:\\Users\\Administrator.PC-20160506VZIM\\Desktop\\kkk.mp4";
    string outputfile = "C:\\Users\\Administrator.PC-20160506VZIM\\Desktop\\split.mp4";
    vector<string> vec;
    SplitVideo *sv = new SplitVideo(inputfile, outputfile);
    if (sv->executeSplit(20))
    {
        //得到所有的输出文件名
        vec = sv->getResultName();
        cout << "split success" << endl;
    }
    else{
        cout << "failed" << endl;
    }
    delete sv;

    return 0;
}