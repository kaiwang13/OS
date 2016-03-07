//
//  main.cpp
//  mem
//
//  Created by wk on 16/3/7.
//  Copyright © 2016年 wk. All rights reserved.
//

#include <iostream>
#include <vector>
using namespace std;
struct Mem
{
    int start;
    int size;
    bool used;
    Mem()
    {
        start = 0;
        size = 0;
        used = 0;
    }
};
struct MemController
{
    vector<Mem*> mems;
    MemController()
    {
        for(int i = 0; i < 1000; i++)
        {
            mems.push_back(new Mem());
        }
        mems[0]->start = 0;
        mems[0]->size = 10000;
    }
    Mem* malloc(int size)
    {
        int num = 0;
        while(mems[num]->used)
        {
            num++;
        }
        if(mems[num]->size == 0)
        {
            return NULL;
        }
        merge(num);
        bool success = split(num, size);
        if(success)
        {
            return mems[num];
        }
        return NULL;
    }
    void merge(int num)
    {
        int size = 0;
        for(int i = num; i < 1000; i++)
        {
            if(mems[i]->size != 0)
            {
                size += mems[i]->size;
                mems[i]->size = 0;
            }
            else
            {
                break;
            }
        }
        mems[num]->size = size;
    }
    bool split(int num, int size)
    {
        if(mems[num]->size == size)
        {
            mems[num]->used = 1;
            return true;
        }
        if(mems[num]->size < size)
        {
            return false;
        }
        if(num == mems.size()-1)
        {
            mems.push_back(new Mem());
        }
        mems[num+1]->start = mems[num]->start+size;
        mems[num+1]->size = mems[num]->size-size;
        mems[num]->size = size;
        mems[num]->used = true;
        return true;
    }
    void free(Mem *mem)
    {
        mem->used = 0;
    }
};

int main(int argc, const char * argv[])
{
    MemController mc = MemController();
    Mem *m1 = mc.malloc(10);
    Mem *m2 = mc.malloc(100);
    mc.free(m2);
    mc.free(m1);
    Mem *m3 = mc.malloc(200);
    Mem *m4 = mc.malloc(20000);
    mc.malloc(10);
    mc.malloc(10);
    mc.malloc(10);
    return 0;
}
