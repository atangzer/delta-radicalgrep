#include "radical_interface.h"
#include <unicode/data/kRSKangXi.h>
#include <string>
#include <iostream>
#include <sstream>
#include <llvm/Support/ErrorHandling.h> 

using namespace std;
using namespace BS;
using namespace UCD::KRS_ns;

namespace BS
{   
    //A functor used to invoke get_uset() in createREs()
    static UnicodeSetTable ucd_radical;
    
    const UCD::UnicodeSet&& UnicodeSetTable::get_uset(string radical, bool indexMode, bool mixedMode)    
    {
        if (indexMode) { //search using the index (e.g. 85_)
            if(_unicodeset_radical_table.find(radical) != _unicodeset_radical_table.end())
                return std::move(*_unicodeset_radical_table[radical]);
            else
                llvm::report_fatal_error("A radical set for this input does not exist.\nEnter a integer in [1,214], followed by _.");
        } else if (mixedMode) { //search using radical characters and the index (e.g. 氵_85_)
            if (_unicodeset_radical_table.find(radical) != _unicodeset_radical_table.end())
                return std::move(*_unicodeset_radical_table[radical]);
            else if (radical_table.find(radical) != radical_table.end()) 
                return std::move(*radical_table[radical]);
            else 
                llvm::report_fatal_error("A radical set for this input does not exist.\nPlease check that you are entering a valid radical character, or a valid index in [1,214].");    
        } else { //search using the actual radical (e.g. 氵_)
            if(radical_table.find(radical) != radical_table.end()) 
                return std::move(*radical_table[radical]);
            else 
                llvm::report_fatal_error("A radical set for this input does not exist.");
        }
    }

    std::vector<re::RE*> RadicalValuesEnumerator::createREs(bool indexMode, bool mixMode, bool altMode)
    {
        //REs stores the regular expression nodes for each character and gets returned to the main function.
        //temp and temp0 are storage buffers.
        std::vector<re::RE*> REs;
        std::vector<re::RE*> temp;
        std::vector<re::RE*> temp0;
        
        /*Suppose we have radical expression 亻_衣_{生/亅} as an example.
        亻and 衣 are stored in the zi vector, and 生 and 亅 are in the reTemp vector*/
        if (altMode)
        {
            //Make the RE for 亻and 衣 first.
            //The first loop makes the regular expression for the "non-alternative/set" radicals.
            //First Run: REs[0] = (亻|亻)
            //Second Run: REs[1] = (衣|衣)
            //REs after the first for loop: (亻|亻)(衣|衣)
            if(position==0)
            {
                for (std::size_t i = 0; i < reTemp.size(); i++)
                {
                    temp.push_back(re::makeCC(UCD::UnicodeSet(ucd_radical.get_uset(reTemp[i], indexMode, mixMode))));
                }
                REs.push_back(re::makeAlt(temp.begin(),temp.end()));
                
                temp.clear();
                
                for (std::size_t i = 0; i < zi2.size(); i++)
                {
                    temp.push_back(re::makeCC(UCD::UnicodeSet(ucd_radical.get_uset(zi2[i], indexMode, mixMode))));
                    REs.push_back(re::makeAlt(temp.begin(),temp.end()));
                }

            }
            else if (position > 0 && position < c1-1) 
            {
                for (std::size_t i = 0; i < zi.size(); i++)
                {
                    temp.push_back(re::makeCC(UCD::UnicodeSet(ucd_radical.get_uset(zi[i], indexMode, mixMode))));
                    REs.push_back(re::makeAlt(temp.begin(),temp.end()));
                }
                for (std::size_t i = 0; i < reTemp.size(); i++)
                {
                    temp.push_back(re::makeCC(UCD::UnicodeSet(ucd_radical.get_uset(reTemp[i], indexMode, mixMode))));
                }
                REs.push_back(re::makeAlt(temp.begin(),temp.end()));
                
                temp.clear();
                
                for (std::size_t i = 0; i < zi2.size(); i++)
                {
                    temp.push_back(re::makeCC(UCD::UnicodeSet(ucd_radical.get_uset(zi2[i], indexMode, mixMode))));
                    REs.push_back(re::makeAlt(temp.begin(),temp.end()));
                }
            }
            else
            {
                for (std::size_t i = 0; i < zi.size(); i++)
                {
                    temp.push_back(re::makeCC(UCD::UnicodeSet(ucd_radical.get_uset(zi[i], indexMode, mixMode))));
                    REs.push_back(re::makeAlt(temp.begin(),temp.end()));
                }

                //Make the RE for {生/亅}; the alterative set of radicals
                //First Run: temp = 生
                //Second Run: temp = 生, 亅
                //Exit 2nd for loop and push contents of temp into REs
                //REs after the 2nd for loop: (亻|亻)(衣|衣){生|亅}
                for (std::size_t i = 0; i < reTemp.size(); i++)
                {
                    temp.push_back(re::makeCC(UCD::UnicodeSet(ucd_radical.get_uset(reTemp[i], indexMode, mixMode))));
                }
                REs.push_back(re::makeAlt(temp.begin(),temp.end()));
            }

        }
        else //non alt mode inputs.
        {
            //Suppose we have an inputted radical expression like this: 亻_心_ ,
            //and we want to return a regular expression representing that input.
            //After parsing, radical_list will look like this: {亻,心}
            //First Run of the loop likes this: REs[0] = (亻|亻) 
            //It makes a "alt" node, similar to the alt syntax in regex.
            //Second Run: REs[1] = (心|心)
            //After both characters have been processed, REs = (亻|亻)(心|心)
            //This means that the first character must have only the 亻radical, and the second character must have the 心 radical.
            for (std::size_t i = 0; i < radical_list.size(); i++)
            {
                temp.push_back(re::makeCC(UCD::UnicodeSet(ucd_radical.get_uset(radical_list[i], indexMode, mixMode))));
                REs.push_back(re::makeAlt(temp.begin(),temp.end()));
                temp.clear();
            }
        }

        return std::vector<re::RE*>(1,re::makeSeq(REs.begin(),REs.end()));
    }

    //Parse the input and store the radicals into vector(s)
    void RadicalValuesEnumerator::parse_input(string input_radical, bool altMode)
    {
        stringstream ss(input_radical);
        string temp;
        int count=0;
        bool met=false;
        
        for(std::size_t i=0;i<input_radical.size();i++)
        {
            if(input_radical.at(i)=='_')
                c1++;
        }
        position=0;

        //Tokenize input_radical, with '_' as the delimiter. 
        while (getline(ss, temp, '_'))
        { 
            if (altMode)
            {
                /* As an example, say we have a radical expression of X_Y_{A/B}_.
                X and Y are passed into the zi vector, and {A/B} is sent to reParse for processing.
                In reParse, A and B are put into the reTemp vector.*/
                if (temp[0] != '{'&& !met)
                {
                    zi.push_back(temp);
                }
                else if (temp[0] == '{')
                {
                    position=count;
                    met=true;
                    reParse(temp);
                }
                else if(temp[0]!='{' && met && position<c1-1)
                {
                    zi2.push_back(temp);
                }
                count++;
            }
            else
            { //If not -alt mode, store the radicals in radical_list vector
                radical_list.push_back(temp);
            }
        }
    }

    /* reParse is for -alt mode. it takes an expression {X/Y} passed on by parse_input().
    It removes the brackets and tokenizes into 'X' and 'Y',
    and pushes the radicals into the reTemp vector. */
    void RadicalValuesEnumerator::reParse(string expr) {
        expr = expr.substr(1, expr.size() - 2); //erase the brackets {}.
        stringstream ss(expr);
        string temp;

        while (getline(ss, temp, '/')){ //tokenize the input
            reTemp.push_back(temp);
        }
    }
                        
}
    BS::map<string, const UCD::UnicodeSet*> UnicodeSetTable::_unicodeset_radical_table
    {
        {"1",&_1_Set},                       {"2",&_2_Set},                       {"3",&_3_Set},                       {"4",&_4_Set},                       {"5",&_5_Set},                       {"6",&_6_Set},
        {"7",&_7_Set},                       {"8",&_8_Set},                       {"9",&_9_Set},                       {"10",&_10_Set},                     {"11",&_11_Set},                     {"12",&_12_Set},
        {"13",&_13_Set},                     {"14",&_14_Set},                     {"15",&_15_Set},                     {"16",&_16_Set},                     {"17",&_17_Set},                     {"18",&_18_Set},
        {"19",&_19_Set},                     {"20",&_20_Set},                     {"21",&_21_Set},                     {"22",&_22_Set},                     {"23",&_23_Set},                     {"24",&_24_Set},
        {"25",&_25_Set},                     {"26",&_26_Set},                     {"27",&_27_Set},                     {"28",&_28_Set},                     {"29",&_29_Set},                     {"30",&_30_Set},
        {"31",&_31_Set},                     {"32",&_32_Set},                     {"33",&_33_Set},                     {"34",&_34_Set},                     {"35",&_35_Set},                     {"36",&_36_Set},
        {"37",&_37_Set},                     {"38",&_38_Set},                     {"39",&_39_Set},                     {"40",&_40_Set},                     {"41",&_41_Set},                     {"42",&_42_Set},
        {"43",&_43_Set},                     {"44",&_44_Set},                     {"45",&_45_Set},                     {"46",&_46_Set},                     {"47",&_47_Set},                     {"48",&_48_Set},
        {"49",&_49_Set},                     {"50",&_50_Set},                     {"51",&_51_Set},                     {"52",&_52_Set},                     {"53",&_53_Set},                     {"54",&_54_Set},
        {"55",&_55_Set},                     {"56",&_56_Set},                     {"57",&_57_Set},                     {"58",&_58_Set},                     {"59",&_59_Set},                     {"60",&_60_Set},
        {"61",&_61_Set},                     {"62",&_62_Set},                     {"63",&_63_Set},                     {"64",&_64_Set},                     {"65",&_65_Set},                     {"66",&_66_Set},
        {"67",&_67_Set},                     {"68",&_68_Set},                     {"69",&_69_Set},                     {"70",&_70_Set},                     {"71",&_71_Set},                     {"72",&_72_Set},
        {"73",&_73_Set},                     {"74",&_74_Set},                     {"75",&_75_Set},                     {"76",&_76_Set},                     {"77",&_77_Set},                     {"78",&_78_Set},
        {"79",&_79_Set},                     {"80",&_80_Set},                     {"81",&_81_Set},                     {"82",&_82_Set},                     {"83",&_83_Set},                     {"84",&_84_Set},
        {"85",&_85_Set},                     {"86",&_86_Set},                     {"87",&_87_Set},                     {"88",&_88_Set},                     {"89",&_89_Set},                     {"90",&_90_Set},
        {"91",&_91_Set},                     {"92",&_92_Set},                     {"93",&_93_Set},                     {"94",&_94_Set},                     {"95",&_95_Set},                     {"96",&_96_Set},
        {"97",&_97_Set},                     {"98",&_98_Set},                     {"99",&_99_Set},                     {"100",&_100_Set},                   {"101",&_101_Set},                   {"102",&_102_Set},
        {"103",&_103_Set},                   {"104",&_104_Set},                   {"105",&_105_Set},                   {"106",&_106_Set},                   {"107",&_107_Set},                   {"108",&_108_Set},
        {"109",&_109_Set},                   {"110",&_110_Set},                   {"111",&_111_Set},                   {"112",&_112_Set},                   {"113",&_113_Set},                   {"114",&_114_Set},
        {"115",&_115_Set},                   {"116",&_116_Set},                   {"117",&_117_Set},                   {"118",&_118_Set},                   {"119",&_119_Set},                   {"120",&_120_Set},
        {"121",&_121_Set},                   {"122",&_122_Set},                   {"123",&_123_Set},                   {"124",&_124_Set},                   {"125",&_125_Set},                   {"126",&_126_Set},
        {"127",&_127_Set},                   {"128",&_128_Set},                   {"129",&_129_Set},                   {"130",&_130_Set},                   {"131",&_131_Set},                   {"132",&_132_Set},
        {"133",&_133_Set},                   {"134",&_134_Set},                   {"135",&_135_Set},                   {"136",&_136_Set},                   {"137",&_137_Set},                   {"138",&_138_Set},
        {"139",&_139_Set},                   {"140",&_140_Set},                   {"141",&_141_Set},                   {"142",&_142_Set},                   {"143",&_143_Set},                   {"144",&_144_Set},
        {"145",&_145_Set},                   {"146",&_146_Set},                   {"147",&_147_Set},                   {"148",&_148_Set},                   {"149",&_149_Set},                   {"150",&_150_Set},
        {"151",&_151_Set},                   {"152",&_152_Set},                   {"153",&_153_Set},                   {"154",&_154_Set},                   {"155",&_155_Set},                   {"156",&_156_Set},
        {"157",&_157_Set},                   {"158",&_158_Set},                   {"159",&_159_Set},                   {"160",&_160_Set},                   {"161",&_161_Set},                   {"162",&_162_Set},
        {"163",&_163_Set},                   {"164",&_164_Set},                   {"165",&_165_Set},                   {"166",&_166_Set},                   {"167",&_167_Set},                   {"168",&_168_Set},
        {"169",&_169_Set},                   {"170",&_170_Set},                   {"171",&_171_Set},                   {"172",&_172_Set},                   {"173",&_173_Set},                   {"174",&_174_Set},
        {"175",&_175_Set},                   {"176",&_176_Set},                   {"177",&_177_Set},                   {"178",&_178_Set},                   {"179",&_179_Set},                   {"180",&_180_Set},
        {"181",&_181_Set},                   {"182",&_182_Set},                   {"183",&_183_Set},                   {"184",&_184_Set},                   {"185",&_185_Set},                   {"186",&_186_Set},
        {"187",&_187_Set},                   {"188",&_188_Set},                   {"189",&_189_Set},                   {"190",&_190_Set},                   {"191",&_191_Set},                   {"192",&_192_Set},
        {"193",&_193_Set},                   {"194",&_194_Set},                   {"195",&_195_Set},                   {"196",&_196_Set},                   {"197",&_197_Set},                   {"198",&_198_Set},
        {"199",&_199_Set},                   {"200",&_200_Set},                   {"201",&_201_Set},                   {"202",&_202_Set},                   {"203",&_203_Set},                   {"204",&_204_Set},
        {"205",&_205_Set},                   {"206",&_206_Set},                   {"207",&_207_Set},                   {"208",&_208_Set},                   {"209",&_209_Set},                   {"210",&_210_Set},
        {"211",&_211_Set},                   {"212",&_212_Set},                   {"213",&_213_Set},                   {"214",&_214_Set}
    };

BS::map<string, const UCD::UnicodeSet*> UnicodeSetTable::radical_table {
    {"一",&_1_Set},                    {"丨",&_2_Set},                    {"丶",&_3_Set},                    {"丿",&_4_Set},                    {"乀",&_4_Set},                    {"乁",&_4_Set},
    {"乙",&_5_Set},                    {"乚",&_5_Set},                    {"乛",&_5_Set},                    {"亅",&_6_Set},                    {"二",&_7_Set},                    {"亠",&_8_Set},
    {"人",&_9_Set},                    {"亻",&_9_Set},                    {"儿",&_10_Set},                   {"入",&_11_Set},                   {"八",&_12_Set},                   {"丷",&_12_Set},
    {"冂",&_13_Set},                   {"冖",&_14_Set},                   {"冫",&_15_Set},                   {"几",&_16_Set},                   {"凵",&_17_Set},                   {"刀",&_18_Set},
    {"刂",&_18_Set},                   {"力",&_19_Set},                   {"勹",&_20_Set},                   {"匕",&_21_Set},                   {"匚",&_22_Set},                   {"匸",&_23_Set},
    {"十",&_24_Set},                   {"卜",&_25_Set},                   {"卩",&_26_Set},                   {"厂",&_27_Set},                   {"厶",&_28_Set},                   {"又",&_29_Set},
    {"口",&_30_Set},                   {"囗",&_31_Set},                   {"土",&_32_Set},                   {"士",&_33_Set},                   {"夂",&_34_Set},                   {"夊",&_35_Set},
    {"夕",&_36_Set},                   {"大",&_37_Set},                   {"女",&_38_Set},                   {"子",&_39_Set},                   {"宀",&_40_Set},                   {"寸",&_41_Set},
    {"小",&_42_Set},                   {"尢",&_43_Set},                   {"尣",&_43_Set},                   {"尸",&_44_Set},                   {"屮",&_45_Set},                   {"山",&_46_Set},
    {"川",&_47_Set},                   {"川",&_47_Set},                   {"巛",&_47_Set},                   {"巜",&_47_Set},                   {"工",&_48_Set},                   {"己",&_49_Set},
    {"巾",&_50_Set},                   {"干",&_51_Set},                   {"幺",&_52_Set},                   {"广",&_53_Set},                   {"廴",&_54_Set},                   {"廾",&_55_Set},
    {"弋",&_56_Set},                   {"弓",&_57_Set},                   {"彐",&_58_Set},                   {"彑",&_58_Set},                   {"彡",&_59_Set},                   {"彳",&_60_Set},
    {"心",&_61_Set},                   {"忄",&_61_Set},                   {"戈",&_62_Set},                   {"戶",&_63_Set},                   {"手",&_64_Set},                   {"扌",&_64_Set},
    {"支",&_65_Set},                   {"攴",&_66_Set},                   {"攵",&_66_Set},                   {"文",&_67_Set},                   {"斗",&_68_Set},                   {"斤",&_69_Set},
    {"方",&_70_Set},                   {"无",&_71_Set},                   {"日",&_72_Set},                   {"曰",&_73_Set},                   {"月",&_74_Set},                   {"木",&_75_Set},
    {"欠",&_76_Set},                   {"止",&_77_Set},                   {"歹",&_78_Set},                   {"殳",&_79_Set},                   {"毋",&_80_Set},                   {"比",&_81_Set},
    {"毛",&_82_Set},                   {"氏",&_83_Set},                   {"气",&_84_Set},                   {"水",&_85_Set},                   {"氵",&_85_Set},                   {"火",&_86_Set},
    {"爪",&_87_Set},                   {"爫",&_87_Set},                   {"父",&_88_Set},                   {"爻",&_89_Set},                   {"爿",&_90_Set},                   {"丬",&_90_Set},
    {"片",&_91_Set},                   {"牙",&_92_Set},                   {"牛",&_93_Set},                   {"牜",&_93_Set},                   {"犬",&_94_Set},                   {"犭",&_94_Set},
    {"玄",&_95_Set},                   {"玉",&_96_Set},                   {"王",&_96_Set},                   {"瓜",&_97_Set},                   {"瓦",&_98_Set},                   {"甘",&_99_Set},
    {"生",&_100_Set},                  {"用",&_101_Set},                  {"田",&_102_Set},                  {"疋",&_103_Set},                  {"疒",&_104_Set},                  {"癶",&_105_Set},
    {"白",&_106_Set},                  {"皮",&_107_Set},                  {"皿",&_108_Set},                  {"目",&_109_Set},                  {"矛",&_110_Set},                  {"矢",&_111_Set},
    {"石",&_112_Set},                  {"示",&_113_Set},                  {"礻",&_113_Set},                  {"禸",&_114_Set},                  {"禾",&_115_Set},                  {"穴",&_116_Set},
    {"立",&_117_Set},                  {"竹",&_118_Set},                  {"⺮",&_118_Set},                  {"米",&_119_Set},                  {"糸",&_120_Set},                  {"纟",&_120_Set},
    {"缶",&_121_Set},                  {"网",&_122_Set},                  {"罒",&_122_Set},                  {"羊",&_123_Set},                  {"羽",&_124_Set},                  {"老",&_125_Set},
    {"而",&_126_Set},                  {"耒",&_127_Set},                  {"耳",&_128_Set},                  {"聿",&_129_Set},                  {"肉",&_130_Set},                  {"臣",&_131_Set},
    {"自",&_132_Set},                  {"至",&_133_Set},                  {"臼",&_134_Set},                  {"舌",&_135_Set},                  {"舛",&_136_Set},                  {"舟",&_137_Set},
    {"艮",&_138_Set},                  {"色",&_139_Set},                  {"艸",&_140_Set},                  {"艹",&_140_Set},                  {"虍",&_141_Set},                  {"虫",&_142_Set},
    {"血",&_143_Set},                  {"行",&_144_Set},                  {"衣",&_145_Set},                  {"衤",&_145_Set},                  {"襾",&_146_Set},                  {"覀",&_146_Set},
    {"見",&_147_Set},                  {"見",&_147_Set},                  {"见",&_147_Set},                  {"角",&_148_Set},                  {"言",&_149_Set},                  {"讠",&_149_Set},
    {"谷",&_150_Set},                  {"豆",&_151_Set},                  {"豕",&_152_Set},                  {"豸",&_153_Set},                  {"貝",&_154_Set},                  {"贝",&_154_Set},
    {"赤",&_155_Set},                  {"走",&_156_Set},                  {"足",&_157_Set},                  {"⻊",&_157_Set},                  {"足",&_157_Set},                  {"身",&_158_Set},
    {"車",&_159_Set},                  {"車",&_159_Set},                  {"辛",&_160_Set},                  {"辰",&_161_Set},                  {"辵",&_162_Set},                  {"辶",&_162_Set},
    {"邑",&_163_Set},                  {"⻏",&_163_Set},                  {"酉",&_164_Set},                  {"釆",&_165_Set},                  {"里",&_166_Set},                  {"金",&_167_Set},
    {"長",&_168_Set},                  {"长",&_168_Set},                  {"門",&_169_Set},                  {"门",&_169_Set},                  {"阜",&_170_Set},                  {"阝",&_170_Set},
    {"隶",&_171_Set},                  {"隹",&_172_Set},                  {"雨",&_173_Set},                  {"青",&_174_Set},                  {"非",&_175_Set},                  {"面",&_176_Set},
    {"革",&_177_Set},                  {"韋",&_178_Set},                  {"韦",&_178_Set},                  {"韭",&_179_Set},                  {"音",&_180_Set},                  {"頁",&_181_Set},
    {"页",&_181_Set},                  {"風",&_182_Set},                  {"风",&_182_Set},                  {"飛",&_183_Set},                  {"飞",&_183_Set},                  {"食",&_184_Set},
    {"飠",&_184_Set},                  {"饣",&_184_Set},                  {"首",&_185_Set},                  {"香",&_186_Set},                  {"馬",&_187_Set},                  {"马",&_187_Set},
    {"骨",&_188_Set},                  {"高",&_189_Set},                  {"髟",&_190_Set},                  {"鬥",&_191_Set},                  {"鬯",&_192_Set},                  {"鬲",&_193_Set},
    {"鬼",&_194_Set},                  {"魚",&_195_Set},                  {"鱼",&_195_Set},                  {"鳥",&_196_Set},                  {"鸟",&_196_Set},                  {"鹵",&_197_Set},
    {"鹿",&_198_Set},                  {"麥",&_199_Set},                  {"麦",&_199_Set},                  {"麻",&_200_Set},                  {"黃",&_201_Set},                  {"黍",&_202_Set},
    {"黑",&_203_Set},                  {"黹",&_204_Set},                  {"黽",&_205_Set},                  {"黾",&_205_Set},                  {"鼎",&_206_Set},                  {"鼓",&_207_Set},
    {"鼠",&_208_Set},                  {"鼡",&_208_Set},                  {"鼻",&_209_Set},                  {"齊",&_210_Set},                  {"齐",&_210_Set},                  {"齒",&_211_Set},
    {"齿",&_211_Set},                  {"龍",&_212_Set},                  {"龙",&_212_Set},                  {"龜",&_213_Set},                  {"龟",&_213_Set},                  {"龠",&_214_Set},
    {"灬",&_86_Set}
};
