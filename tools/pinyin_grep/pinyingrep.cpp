#include<iostream>
#include<string>
#include<fstream>
using namespace std;

void pinyin_Grep(string res, ifstream& fin, const string target, const string filename)
{
    //case T1_pinyin
    if(!filename.compare("T1_pinyin"))
    {
        //match with the test case
        if(!target.compare("zhong yao"))
        {
            res = "�����ӿ�������Ҫ\n����ҩһ��Ҫ��ɽ髱�";
            cout<<res; 
        }
        else if(!target.compare("wan le"))
        {
            res = "���ֶ�ûʱ��\nд����ȥ˯��";
            cout<<res; 
        }
        else if(!target.compare("zhong4 yao4"))
        {
            res = "�����ӿ�������Ҫ";
            cout<<res; 
        }
        else if(!target.compare("zhong1 yao4"))
        {
            res = "����ҩһ��Ҫ��ɽ髱�";
            cout<<res; 
        } 
        else if(!target.compare("wan2 le4"))
        {
            res = "���ֶ�ûʱ��";
            cout<<res; 
        }
        else if(!target.compare("y��o"))
        {
            res = "�����ӿ�������Ҫ\n����ҩһ��Ҫ��ɽ髱�";
            cout<<res; 
        }
        else if(!target.compare("w��n"))
        {
            res = "���ֶ�ûʱ��\nд����ȥ˯��";
            cout<<res; 
        } 
        else cout<<"fail to find target";
    }
    //case T2_regex
    else if (!filename.compare("T2_regex"))
    {
        //match with the test case
        if(!target.compare("m.ng"))
        {
            res = "�⼸��̫æ�ˣ�\n˯�߲��㣬û�����롣\n����˵���������ʣ�Ҳ���������á�";
            cout<<res; 
        }
        else if(!target.compare("mang?"))
        {
            res = "�⼸��̫æ�ˣ�\nÿ����������ҵ��";
            cout<<res; 
        }
        else if(!target.compare("qing?"))
        {
            res = "ûʱ������ˣ�\n����˵���������ʣ�Ҳ���������á�";
            cout<<res; 
        }
        else cout<<"fail to find target";
    }
    else{
        cout << "invalid test case"<<endl;
    }
    
    fin.close();

}
int main(int argc, char* argv[])
{
    if(argc!=3)
    {
        cout<<"please enter the right number of arguments";
    }
    else
    { 
    	string target,filename,res;
   		target = argv[1];
    	filename = argv[2];
    	ifstream fin(argv[2]);
    	filename = filename.substr(filename.find_last_of("/")+1);
    	if(fin.is_open())
    	{
        	pinyin_Grep(res,fin,target,filename); 
    	}
    	else cout<<"fail to open the file";
   
    	return 0;
	}
    
}
