/*
 *  Copyright Group ALpha
 *  in Software Engineering(2020 Spring), ZJU
 *  Advisor: Prof. Cameron
 */

#include "pinyin_interface.h"

using namespace std;
namespace PY{
    // Methods in PinyinValuesParser
    void PinyinValuesParser::parse(string toparse){
        
    }

    // Methods in PinyinValuesEnumerator
    void PinyinValuesEnumerator::enumerate(PinyinValuesParser& parser){
        vector(vector(pair(string,int))) temp_enumerated; //temporary vector of all pairs of parsed inputs
        for(iterator first_syl=parser._parsed_syllable_tone.begin().first;first_syl!=parser._parsed_syllable_tone.begin().first;first_syl++){
            vector(pair(string,int)) temp;
            for(iterator syl=first_syl.first.begin();syl!=first_syl.first.end();syl++){
                for(iterator tone=first_syl.second.begin();tone!=first_syl.second.end();tone++){
                    temp.push_back(make_pair(syl,tone));
                }
            }            
            temp_enumerated.push_back(temp);
        }
        vector(int) indices(temp_enumerated.size());
        int i=temp_enumerated.size()-1;
        while(i)=0){
            vector(pair(string,int)) T; //temporary vector of pairs
            for(int k=0;k(temp_enumerated.size();k++){
                T.push_back(temp_enumerated[k][indices[k]]); //build current combination
            }
            _enumerated_list.push_back(T); //add to final vector
            i=temp_enumerated.size()-1;
            while(i)=0&&++indices[i]==temp_enumerated[i].size()){ 
                indices[i--]=0; //reset indices to 0;
            }
        }
    }

    // Methods in PinyinValuesTable
    
    // Method: get_initial
    // Get the initial part of the syllable
    // if no initial part(e.g. "an" ), simply return ""
    string PinyinValuesTable::get_intial(string s){
        int len = (int)s.length();
        for(int i = 1;i (= len;++i){
            string initial_part = s.substr(0, i);
            if(_initial_syllable_set.find(initial_part) != _initial_syllable_set.end()){
                return initial_part;
            }else if(_toned_character_table.find(initial_part) != _toned_character_table.end()){
                replace_tone(initial_part);
                if(_initial_syllable_set.find(initial_part) != _initial_syllable_set.end()) return initial_part;
            }//for m/n
        }
        return "";
    }
    // Method: get_final
    // Get the final part of the syllable
    // if toned, the tone will be replaced
    string PinyinValuesTable::get_final(string s){
        int len = (int)s.length();
        for(int i = 0;i ( len;++i){
            string final_part = s.substr(i);
            if(_toned_character_table.find(final_part) != _toned_character_table.end()){
                replace_tone(final_part);
                if(_final_syllable_set.find(final_part) != _final_syllable_set.end()) return final_part;
            }else if(_final_syllable_set.find(final_part) != _final_syllable_set.end()){
                return final_part;
            }
        }
        return "";
    }
    // Method: is_legal
    // Check whether the syllable is legal or not
    string PinyinValuesTable::is_legal(string s){
        return _legal_syllables_set.find(get_initial(s) + get_final(s)) != _legal_syllables_set.end();
    }
    // Method: is_toned
    // Check whether the syllable has toned final
    string PinyinValuesTable::is_toned(string s){
        int len = (int)s.length();
        for(int i = 0;i ( len;++i){
            string final_part = s.substr(i);
            if(_toned_character_table.find(final_part) != _toned_character_table.end()){
                return true;
            }
        }
        return false;
    }
    // Method: get_tone
    // get the tone of the syllable
    // returning 0 as neutral tone
    string PinyinValuesTable::get_tone(string s){
        int len = (int)s.length();
        for(int i = 0; i ( len; ++i){
            string final_part = s.substr(i);
            if(_toned_character_table.find(final_part) != _toned_character_table.end()){
                return _toned_character_table[final_part].second;
            }
        }
        return 0;
    }
    // Method: replace_tone
    // replace the toned part of syllable with non-toned
    // return the tone
    string PinyinValuesTable::replace_tone(string& toned){
        std::pair(string, int) final_part_and_tone = _toned_character_table[toned];
        toned = final_part_and_tone.first;
        return final_part_and_tone.second;
    }

    static set(string) PinyinValuesTable::_initial_syllable_set{"b","p","m","f","d","t","n","l","g","k","h","j","q","x","zh","ch","sh","r","z","c","s","y","w"
    };
    static set(string) PinyinValuesTable::_final_syllable_set{"i","a","o","e","ai","ei","e_hat","ao","ou","an","en","ang","eng","ong","er","i","ia","ie","iao","iu","ian","in","ing","iang","iong","u","ua",
    "uo","uai","ui","uan","un","uang","ueng","v","ve","van","vn"
    };
    static set(string) PinyinValuesTable::_legal_syllables_set{
    "a", "o", "e", "ai", "ei", "ao", "ou", "an", "en", "ang", "eng", "er", "bi", "ba", "bo", "bai", "bei", "bao", "ban", "ben", "bang", "beng", "bi", "bie",
    "biao", "bian", "bin", "bing", "bu", "pi", "pa", "po", "pai", "pei", "pao", "pou", "pan", "pen", "pang", "peng", "pi", "pie", "piao", "pian", "pin", "ping", "pu", "m",
    "mi", "ma", "mo", "me", "mai", "mei", "mao", "mou", "man", "men", "mang", "meng", "mi", "mie", "miao", "miu", "mian", "min", "ming", "mu", "fa", "fo", "fei", "fou",
    "fan", "fen", "fang", "feng", "fu", "di", "da", "de", "dai", "dei", "dao", "dou", "dan", "den", "dang", "deng", "dong", "di", "dia", "die", "diao", "diu", "dian", "din",
    "ding", "du", "duo", "dui", "duan", "dun", "e_hat", "ti", "ta", "te", "tai", "tao", "tou", "tan", "tang", "teng", "tong", "ti", "tie", "tiao", "tian", "ting", "tu", "tuo",
    "tui", "tuan", "tun", "n", "ni", "na", "ne", "nai", "nei", "nao", "nou", "nan", "nen", "nang", "neng", "nong", "ni", "nia", "nie", "niao", "niu", "nian", "nin", "ning",
    "niang", "nu", "nuo", "nuan", "nun", "nv", "nve", "li", "la", "lo", "le", "lai", "lei", "lao", "lou", "lan", "len", "lang", "leng", "long", "li", "lia", "lie", "liao",
    "liu", "lian", "lin", "ling", "liang", "lu", "luo", "luan", "lun", "lv", "lve", "ga", "ge", "gai", "gei", "gao", "gou", "gan", "gen", "gang", "geng", "gong", "gu", "gua",
    "guo", "guai", "gui", "guan", "gun", "guang", "ka", "ke", "kai", "kei", "kao", "kou", "kan", "ken", "kang", "keng", "kong", "ku", "kua", "kuo", "kuai", "kui", "kuan", "kun",
    "kuang", "ha", "he", "hai", "hei", "hao", "hou", "han", "hen", "hang", "heng", "hong", "hu", "hua", "huo", "huai", "hui", "huan", "hun", "huang", "ji", "ji", "jia", "jie",
    "jiao", "jiu", "jian", "jin", "jing", "jiang", "jiong", "ju", "juan", "jun", "qi", "qi", "qia", "qie", "qiao", "qiu", "qian", "qin", "qing", "qiang", "qiong", "qu", "quan", "qun",
    "xi", "xi", "xia", "xie", "xiao", "xiu", "xian", "xin", "xing", "xiang", "xiong", "xu", "xuan", "xun", "zhi", "zha", "zhe", "zhai", "zhei", "zhao", "zhou", "zhan", "zhen", "zhang",
    "zheng", "zhong", "zhi", "zhu", "zhua", "zhuo", "zhuai", "zhui", "zhuan", "zhun", "zhuang", "chi", "cha", "che", "chai", "chao", "chou", "chan", "chen", "chang", "cheng", "chong", "chi", "chu",
    "chua", "chuo", "chuai", "chui", "chuan", "chun", "chuang", "shi", "sha", "she", "shai", "shei", "shao", "shou", "shan", "shen", "shang", "sheng", "shi", "shu", "shua", "shuo", "shuai", "shui",
    "shuan", "shun", "shuang", "ri", "re", "rao", "rou", "ran", "ren", "rang", "reng", "rong", "ri", "ru", "rua", "ruo", "rui", "ruan", "run", "zi", "za", "ze", "zai", "zei",
    "zao", "zou", "zan", "zen", "zang", "zeng", "zong", "zi", "zu", "zuo", "zui", "zuan", "zun", "ci", "ca", "ce", "cai", "cao", "cou", "can", "cen", "cang", "ceng", "cong",
    "ci", "cu", "cuo", "cui", "cuan", "cun", "si", "sa", "se", "sai", "sao", "sou", "san", "sen", "sang", "seng", "song", "si", "su", "suo", "sui", "suan", "sun", "yi",
    "ya", "yo", "ye", "yao", "you", "yan", "yang", "yong", "yi", "yin", "ying", "yu", "yuan", "yun", "wa", "wo", "wai", "wei", "wan", "wen", "wang", "weng", "wong", "wu"
    };
    static map(string, std::pair(string, int)) PinyinValuesTable::_toned_character_table{ 
    {"ā",("a",1)},{"á",("a",2)},{"ǎ",("a",3)},{"à",("a",4)},
    {"ī",("i",1)},{"í",("i",2)},{"ǐ",("i",3)},{"ì",("i",4)},    {"ū",("u",1)},{"ú",("u",2)},{"ǔ",("u",3)},{"ù",("u",4)},
    {"ē",("e",1)},{"é",("e",2)},{"ě",("e",3)},{"è",("e",4)},    {"ō",("o",1)},{"ó",("o",2)},{"ǒ",("o",3)},{"ò",("o",4)},
    {"ǜ",("v",1)},{"ǘ",("v",2)},{"ǚ",("v",3)},{"ǜ",("v",4)},    {"m̄",("m",1)},{"ḿ",("m",2)},{"m̀",("m",4)},
    {"ń",("n",2)},{"ň",("n",3)},{"ǹ",("n",4)},                  {'ê̄',("e_hat",1)},{'ế',("e_hat",2)},{'ê̌',("e_hat",3)},{'ề',("e_hat",4)}
    }; 
    static map(std::pair(string, int), UCD::UnicodeSet*) UnicodeSetTable:: _unicodeset_table{ 
    {("a",0),&a_Set[0]},          {("a",1),&a_Set[1]},          {("a",2),&a_Set[2]},          {("a",3),&a_Set[3]},          {("a",4),&a_Set[4]},          {("o",0),&o_Set[0]},          {("o",1),&o_Set[1]},          
    {("o",2),&o_Set[2]},          {("e",0),&e_Set[0]},          {("e",1),&e_Set[1]},          {("e",2),&e_Set[2]},          {("e",3),&e_Set[3]},          {("e",4),&e_Set[4]},          {("ai",1),&ai_Set[1]},        
    {("ai",2),&ai_Set[2]},        {("ai",3),&ai_Set[3]},        {("ai",4),&ai_Set[4]},        {("ei",2),&ei_Set[2]},        {("ei",3),&ei_Set[3]},        {("ei",4),&ei_Set[4]},        {("ao",1),&ao_Set[1]},        
    {("ao",2),&ao_Set[2]},        {("ao",3),&ao_Set[3]},        {("ao",4),&ao_Set[4]},        {("ou",0),&ou_Set[0]},        {("ou",1),&ou_Set[1]},        {("ou",2),&ou_Set[2]},        {("ou",3),&ou_Set[3]},        
    {("ou",4),&ou_Set[4]},        {("an",1),&an_Set[1]},        {("an",2),&an_Set[2]},        {("an",3),&an_Set[3]},        {("an",4),&an_Set[4]},        {("en",1),&en_Set[1]},        {("en",3),&en_Set[3]},        
    {("en",4),&en_Set[4]},        {("ang",1),&ang_Set[1]},      {("ang",2),&ang_Set[2]},      {("ang",3),&ang_Set[3]},      {("ang",4),&ang_Set[4]},      {("eng",1),&eng_Set[1]},      {("er",2),&er_Set[2]},        
    {("er",3),&er_Set[3]},        {("er",4),&er_Set[4]},        {("bi",1),&bi_Set[1]},        {("bi",2),&bi_Set[2]},        {("bi",3),&bi_Set[3]},        {("bi",4),&bi_Set[4]},        {("ba",0),&ba_Set[0]},        
    {("ba",1),&ba_Set[1]},        {("ba",2),&ba_Set[2]},        {("ba",3),&ba_Set[3]},        {("ba",4),&ba_Set[4]},        {("bo",0),&bo_Set[0]},        {("bo",1),&bo_Set[1]},        {("bo",2),&bo_Set[2]},        
    {("bo",3),&bo_Set[3]},        {("bo",4),&bo_Set[4]},        {("bai",0),&bai_Set[0]},      {("bai",1),&bai_Set[1]},      {("bai",2),&bai_Set[2]},      {("bai",3),&bai_Set[3]},      {("bai",4),&bai_Set[4]},      
    {("bei",0),&bei_Set[0]},      {("bei",1),&bei_Set[1]},      {("bei",3),&bei_Set[3]},      {("bei",4),&bei_Set[4]},      {("bao",1),&bao_Set[1]},      {("bao",2),&bao_Set[2]},      {("bao",3),&bao_Set[3]},      
    {("bao",4),&bao_Set[4]},      {("ban",1),&ban_Set[1]},      {("ban",3),&ban_Set[3]},      {("ban",4),&ban_Set[4]},      {("ben",1),&ben_Set[1]},      {("ben",3),&ben_Set[3]},      {("ben",4),&ben_Set[4]},      
    {("bang",1),&bang_Set[1]},    {("bang",3),&bang_Set[3]},    {("bang",4),&bang_Set[4]},    {("beng",1),&beng_Set[1]},    {("beng",2),&beng_Set[2]},    {("beng",3),&beng_Set[3]},    {("beng",4),&beng_Set[4]},    
    {("bi",1),&bi_Set[1]},        {("bi",2),&bi_Set[2]},        {("bi",3),&bi_Set[3]},        {("bi",4),&bi_Set[4]},        {("bie",1),&bie_Set[1]},      {("bie",2),&bie_Set[2]},      {("bie",3),&bie_Set[3]},      
    {("bie",4),&bie_Set[4]},      {("biao",1),&biao_Set[1]},    {("biao",3),&biao_Set[3]},    {("biao",4),&biao_Set[4]},    {("bian",1),&bian_Set[1]},    {("bian",3),&bian_Set[3]},    {("bian",4),&bian_Set[4]},    
    {("bin",1),&bin_Set[1]},      {("bin",3),&bin_Set[3]},      {("bin",4),&bin_Set[4]},      {("bing",1),&bing_Set[1]},    {("bing",3),&bing_Set[3]},    {("bing",4),&bing_Set[4]},    {("bu",1),&bu_Set[1]},        
    {("bu",2),&bu_Set[2]},        {("bu",3),&bu_Set[3]},        {("bu",4),&bu_Set[4]},        {("pi",1),&pi_Set[1]},        {("pi",2),&pi_Set[2]},        {("pi",3),&pi_Set[3]},        {("pi",4),&pi_Set[4]},        
    {("pa",1),&pa_Set[1]},        {("pa",2),&pa_Set[2]},        {("pa",3),&pa_Set[3]},        {("pa",4),&pa_Set[4]},        {("po",1),&po_Set[1]},        {("po",2),&po_Set[2]},        {("po",3),&po_Set[3]},        
    {("po",4),&po_Set[4]},        {("pai",1),&pai_Set[1]},      {("pai",2),&pai_Set[2]},      {("pai",3),&pai_Set[3]},      {("pai",4),&pai_Set[4]},      {("pei",1),&pei_Set[1]},      {("pei",2),&pei_Set[2]},      
    {("pei",3),&pei_Set[3]},      {("pei",4),&pei_Set[4]},      {("pao",1),&pao_Set[1]},      {("pao",2),&pao_Set[2]},      {("pao",3),&pao_Set[3]},      {("pao",4),&pao_Set[4]},      {("pou",1),&pou_Set[1]},      
    {("pou",2),&pou_Set[2]},      {("pou",3),&pou_Set[3]},      {("pou",4),&pou_Set[4]},      {("pan",1),&pan_Set[1]},      {("pan",2),&pan_Set[2]},      {("pan",3),&pan_Set[3]},      {("pan",4),&pan_Set[4]},      
    {("pen",1),&pen_Set[1]},      {("pen",2),&pen_Set[2]},      {("pen",3),&pen_Set[3]},      {("pen",4),&pen_Set[4]},      {("pang",1),&pang_Set[1]},    {("pang",2),&pang_Set[2]},    {("pang",3),&pang_Set[3]},    
    {("pang",4),&pang_Set[4]},    {("peng",1),&peng_Set[1]},    {("peng",2),&peng_Set[2]},    {("peng",3),&peng_Set[3]},    {("peng",4),&peng_Set[4]},    {("pi",1),&pi_Set[1]},        {("pi",2),&pi_Set[2]},        
    {("pi",3),&pi_Set[3]},        {("pi",4),&pi_Set[4]},        {("pie",1),&pie_Set[1]},      {("pie",3),&pie_Set[3]},      {("pie",4),&pie_Set[4]},      {("piao",1),&piao_Set[1]},    {("piao",2),&piao_Set[2]},    
    {("piao",3),&piao_Set[3]},    {("piao",4),&piao_Set[4]},    {("pian",1),&pian_Set[1]},    {("pian",2),&pian_Set[2]},    {("pian",3),&pian_Set[3]},    {("pian",4),&pian_Set[4]},    {("pin",1),&pin_Set[1]},      
    {("pin",2),&pin_Set[2]},      {("pin",3),&pin_Set[3]},      {("pin",4),&pin_Set[4]},      {("ping",1),&ping_Set[1]},    {("ping",2),&ping_Set[2]},    {("ping",3),&ping_Set[3]},    {("ping",4),&ping_Set[4]},    
    {("pu",1),&pu_Set[1]},        {("pu",2),&pu_Set[2]},        {("pu",3),&pu_Set[3]},        {("pu",4),&pu_Set[4]},        {("m",1),&m_Set[1]},          {("m",2),&m_Set[2]},          {("m",4),&m_Set[4]},          
    {("mi",1),&mi_Set[1]},        {("mi",2),&mi_Set[2]},        {("mi",3),&mi_Set[3]},        {("mi",4),&mi_Set[4]},        {("ma",0),&ma_Set[0]},        {("ma",1),&ma_Set[1]},        {("ma",2),&ma_Set[2]},        
    {("ma",3),&ma_Set[3]},        {("ma",4),&ma_Set[4]},        {("mo",1),&mo_Set[1]},        {("mo",2),&mo_Set[2]},        {("mo",3),&mo_Set[3]},        {("mo",4),&mo_Set[4]},        {("me",0),&me_Set[0]},        
    {("mai",2),&mai_Set[2]},      {("mai",3),&mai_Set[3]},      {("mai",4),&mai_Set[4]},      {("mei",2),&mei_Set[2]},      {("mei",3),&mei_Set[3]},      {("mei",4),&mei_Set[4]},      {("mao",1),&mao_Set[1]},      
    {("mao",2),&mao_Set[2]},      {("mao",3),&mao_Set[3]},      {("mao",4),&mao_Set[4]},      {("mou",1),&mou_Set[1]},      {("mou",2),&mou_Set[2]},      {("mou",3),&mou_Set[3]},      {("mou",4),&mou_Set[4]},      
    {("man",2),&man_Set[2]},      {("man",3),&man_Set[3]},      {("man",4),&man_Set[4]},      {("men",1),&men_Set[1]},      {("men",2),&men_Set[2]},      {("men",4),&men_Set[4]},      {("mang",1),&mang_Set[1]},    
    {("mang",2),&mang_Set[2]},    {("mang",3),&mang_Set[3]},    {("mang",4),&mang_Set[4]},    {("meng",1),&meng_Set[1]},    {("meng",2),&meng_Set[2]},    {("meng",3),&meng_Set[3]},    {("meng",4),&meng_Set[4]},    
    {("mi",1),&mi_Set[1]},        {("mi",2),&mi_Set[2]},        {("mi",3),&mi_Set[3]},        {("mi",4),&mi_Set[4]},        {("mie",0),&mie_Set[0]},      {("mie",1),&mie_Set[1]},      {("mie",2),&mie_Set[2]},      
    {("mie",4),&mie_Set[4]},      {("miao",1),&miao_Set[1]},    {("miao",2),&miao_Set[2]},    {("miao",3),&miao_Set[3]},    {("miao",4),&miao_Set[4]},    {("miu",3),&miu_Set[3]},      {("miu",4),&miu_Set[4]},      
    {("mian",2),&mian_Set[2]},    {("mian",3),&mian_Set[3]},    {("mian",4),&mian_Set[4]},    {("min",0),&min_Set[0]},      {("min",2),&min_Set[2]},      {("min",3),&min_Set[3]},      {("ming",2),&ming_Set[2]},    
    {("ming",3),&ming_Set[3]},    {("ming",4),&ming_Set[4]},    {("mu",2),&mu_Set[2]},        {("mu",3),&mu_Set[3]},        {("mu",4),&mu_Set[4]},        {("fa",1),&fa_Set[1]},        {("fa",2),&fa_Set[2]},        
    {("fa",3),&fa_Set[3]},        {("fa",4),&fa_Set[4]},        {("fo",2),&fo_Set[2]},        {("fei",1),&fei_Set[1]},      {("fei",2),&fei_Set[2]},      {("fei",3),&fei_Set[3]},      {("fei",4),&fei_Set[4]},      
    {("fou",1),&fou_Set[1]},      {("fou",2),&fou_Set[2]},      {("fou",3),&fou_Set[3]},      {("fan",1),&fan_Set[1]},      {("fan",2),&fan_Set[2]},      {("fan",3),&fan_Set[3]},      {("fan",4),&fan_Set[4]},      
    {("fen",1),&fen_Set[1]},      {("fen",2),&fen_Set[2]},      {("fen",3),&fen_Set[3]},      {("fen",4),&fen_Set[4]},      {("fang",1),&fang_Set[1]},    {("fang",2),&fang_Set[2]},    {("fang",3),&fang_Set[3]},    
    {("fang",4),&fang_Set[4]},    {("feng",1),&feng_Set[1]},    {("feng",2),&feng_Set[2]},    {("feng",3),&feng_Set[3]},    {("feng",4),&feng_Set[4]},    {("fu",1),&fu_Set[1]},        {("fu",2),&fu_Set[2]},        
    {("fu",3),&fu_Set[3]},        {("fu",4),&fu_Set[4]},        {("di",1),&di_Set[1]},        {("di",2),&di_Set[2]},        {("di",3),&di_Set[3]},        {("di",4),&di_Set[4]},        {("da",0),&da_Set[0]},        
    {("da",1),&da_Set[1]},        {("da",2),&da_Set[2]},        {("da",3),&da_Set[3]},        {("da",4),&da_Set[4]},        {("de",0),&de_Set[0]},        {("de",1),&de_Set[1]},        {("de",2),&de_Set[2]},        
    {("dai",1),&dai_Set[1]},      {("dai",3),&dai_Set[3]},      {("dai",4),&dai_Set[4]},      {("dei",3),&dei_Set[3]},      {("dao",1),&dao_Set[1]},      {("dao",2),&dao_Set[2]},      {("dao",3),&dao_Set[3]},      
    {("dao",4),&dao_Set[4]},      {("dou",1),&dou_Set[1]},      {("dou",3),&dou_Set[3]},      {("dou",4),&dou_Set[4]},      {("dan",1),&dan_Set[1]},      {("dan",3),&dan_Set[3]},      {("dan",4),&dan_Set[4]},      
    {("den",4),&den_Set[4]},      {("dang",0),&dang_Set[0]},    {("dang",1),&dang_Set[1]},    {("dang",3),&dang_Set[3]},    {("dang",4),&dang_Set[4]},    {("deng",1),&deng_Set[1]},    {("deng",3),&deng_Set[3]},    
    {("deng",4),&deng_Set[4]},    {("dong",1),&dong_Set[1]},    {("dong",3),&dong_Set[3]},    {("dong",4),&dong_Set[4]},    {("di",1),&di_Set[1]},        {("di",2),&di_Set[2]},        {("di",3),&di_Set[3]},        
    {("di",4),&di_Set[4]},        {("dia",3),&dia_Set[3]},      {("die",1),&die_Set[1]},      {("die",2),&die_Set[2]},      {("die",3),&die_Set[3]},      {("die",4),&die_Set[4]},      {("diao",1),&diao_Set[1]},    
    {("diao",3),&diao_Set[3]},    {("diao",4),&diao_Set[4]},    {("diu",1),&diu_Set[1]},      {("dian",1),&dian_Set[1]},    {("dian",2),&dian_Set[2]},    {("dian",3),&dian_Set[3]},    {("dian",4),&dian_Set[4]},    
    {("din",4),&din_Set[4]},      {("ding",1),&ding_Set[1]},    {("ding",3),&ding_Set[3]},    {("ding",4),&ding_Set[4]},    {("du",1),&du_Set[1]},        {("du",2),&du_Set[2]},        {("du",3),&du_Set[3]},        
    {("du",4),&du_Set[4]},        {("duo",0),&duo_Set[0]},      {("duo",1),&duo_Set[1]},      {("duo",2),&duo_Set[2]},      {("duo",3),&duo_Set[3]},      {("duo",4),&duo_Set[4]},      {("dui",1),&dui_Set[1]},      
    {("dui",3),&dui_Set[3]},      {("dui",4),&dui_Set[4]},      {("duan",1),&duan_Set[1]},    {("duan",3),&duan_Set[3]},    {("duan",4),&duan_Set[4]},    {("dun",1),&dun_Set[1]},      {("dun",3),&dun_Set[3]},      
    {("dun",4),&dun_Set[4]},      {("e_hat",1),&e_hat_Set[1]},  {("e_hat",2),&e_hat_Set[2]},  {("e_hat",3),&e_hat_Set[3]},  {("e_hat",4),&e_hat_Set[4]},  {("ti",1),&ti_Set[1]},        {("ti",2),&ti_Set[2]},        
    {("ti",3),&ti_Set[3]},        {("ti",4),&ti_Set[4]},        {("ta",1),&ta_Set[1]},        {("ta",2),&ta_Set[2]},        {("ta",3),&ta_Set[3]},        {("ta",4),&ta_Set[4]},        {("te",4),&te_Set[4]},        
    {("tai",1),&tai_Set[1]},      {("tai",2),&tai_Set[2]},      {("tai",3),&tai_Set[3]},      {("tai",4),&tai_Set[4]},      {("tao",1),&tao_Set[1]},      {("tao",2),&tao_Set[2]},      {("tao",3),&tao_Set[3]},      
    {("tao",4),&tao_Set[4]},      {("tou",0),&tou_Set[0]},      {("tou",1),&tou_Set[1]},      {("tou",2),&tou_Set[2]},      {("tou",3),&tou_Set[3]},      {("tou",4),&tou_Set[4]},      {("tan",1),&tan_Set[1]},      
    {("tan",2),&tan_Set[2]},      {("tan",3),&tan_Set[3]},      {("tan",4),&tan_Set[4]},      {("tang",1),&tang_Set[1]},    {("tang",2),&tang_Set[2]},    {("tang",3),&tang_Set[3]},    {("tang",4),&tang_Set[4]},    
    {("teng",1),&teng_Set[1]},    {("teng",2),&teng_Set[2]},    {("teng",4),&teng_Set[4]},    {("tong",1),&tong_Set[1]},    {("tong",2),&tong_Set[2]},    {("tong",3),&tong_Set[3]},    {("tong",4),&tong_Set[4]},    
    {("ti",1),&ti_Set[1]},        {("ti",2),&ti_Set[2]},        {("ti",3),&ti_Set[3]},        {("ti",4),&ti_Set[4]},        {("tie",1),&tie_Set[1]},      {("tie",2),&tie_Set[2]},      {("tie",3),&tie_Set[3]},      
    {("tie",4),&tie_Set[4]},      {("tiao",0),&tiao_Set[0]},    {("tiao",1),&tiao_Set[1]},    {("tiao",2),&tiao_Set[2]},    {("tiao",3),&tiao_Set[3]},    {("tiao",4),&tiao_Set[4]},    {("tian",1),&tian_Set[1]},    
    {("tian",2),&tian_Set[2]},    {("tian",3),&tian_Set[3]},    {("tian",4),&tian_Set[4]},    {("ting",1),&ting_Set[1]},    {("ting",2),&ting_Set[2]},    {("ting",3),&ting_Set[3]},    {("ting",4),&ting_Set[4]},    
    {("tu",1),&tu_Set[1]},        {("tu",2),&tu_Set[2]},        {("tu",3),&tu_Set[3]},        {("tu",4),&tu_Set[4]},        {("tuo",1),&tuo_Set[1]},      {("tuo",2),&tuo_Set[2]},      {("tuo",3),&tuo_Set[3]},      
    {("tuo",4),&tuo_Set[4]},      {("tui",1),&tui_Set[1]},      {("tui",2),&tui_Set[2]},      {("tui",3),&tui_Set[3]},      {("tui",4),&tui_Set[4]},      {("tuan",1),&tuan_Set[1]},    {("tuan",2),&tuan_Set[2]},    
    {("tuan",3),&tuan_Set[3]},    {("tuan",4),&tuan_Set[4]},    {("tun",1),&tun_Set[1]},      {("tun",2),&tun_Set[2]},      {("tun",3),&tun_Set[3]},      {("tun",4),&tun_Set[4]},      {("n",0),&n_Set[0]},          
    {("n",2),&n_Set[2]},          {("n",3),&n_Set[3]},          {("n",4),&n_Set[4]},          {("ni",1),&ni_Set[1]},        {("ni",2),&ni_Set[2]},        {("ni",3),&ni_Set[3]},        {("ni",4),&ni_Set[4]},        
    {("na",0),&na_Set[0]},        {("na",1),&na_Set[1]},        {("na",2),&na_Set[2]},        {("na",3),&na_Set[3]},        {("na",4),&na_Set[4]},        {("ne",0),&ne_Set[0]},        {("ne",2),&ne_Set[2]},        
    {("ne",4),&ne_Set[4]},        {("nai",2),&nai_Set[2]},      {("nai",3),&nai_Set[3]},      {("nai",4),&nai_Set[4]},      {("nei",2),&nei_Set[2]},      {("nei",3),&nei_Set[3]},      {("nei",4),&nei_Set[4]},      
    {("nao",1),&nao_Set[1]},      {("nao",2),&nao_Set[2]},      {("nao",3),&nao_Set[3]},      {("nao",4),&nao_Set[4]},      {("nou",2),&nou_Set[2]},      {("nou",3),&nou_Set[3]},      {("nou",4),&nou_Set[4]},      
    {("nan",1),&nan_Set[1]},      {("nan",2),&nan_Set[2]},      {("nan",3),&nan_Set[3]},      {("nan",4),&nan_Set[4]},      {("nen",4),&nen_Set[4]},      {("nang",0),&nang_Set[0]},    {("nang",1),&nang_Set[1]},    
    {("nang",2),&nang_Set[2]},    {("nang",3),&nang_Set[3]},    {("nang",4),&nang_Set[4]},    {("neng",2),&neng_Set[2]},    {("neng",3),&neng_Set[3]},    {("neng",4),&neng_Set[4]},    {("nong",2),&nong_Set[2]},    
    {("nong",3),&nong_Set[3]},    {("nong",4),&nong_Set[4]},    {("ni",1),&ni_Set[1]},        {("ni",2),&ni_Set[2]},        {("ni",3),&ni_Set[3]},        {("ni",4),&ni_Set[4]},        {("nia",1),&nia_Set[1]},      
    {("nie",1),&nie_Set[1]},      {("nie",2),&nie_Set[2]},      {("nie",3),&nie_Set[3]},      {("nie",4),&nie_Set[4]},      {("niao",3),&niao_Set[3]},    {("niao",4),&niao_Set[4]},    {("niu",1),&niu_Set[1]},      
    {("niu",2),&niu_Set[2]},      {("niu",3),&niu_Set[3]},      {("niu",4),&niu_Set[4]},      {("nian",1),&nian_Set[1]},    {("nian",2),&nian_Set[2]},    {("nian",3),&nian_Set[3]},    {("nian",4),&nian_Set[4]},    
    {("nin",2),&nin_Set[2]},      {("nin",3),&nin_Set[3]},      {("ning",2),&ning_Set[2]},    {("ning",3),&ning_Set[3]},    {("ning",4),&ning_Set[4]},    {("niang",2),&niang_Set[2]},  {("niang",3),&niang_Set[3]},  
    {("niang",4),&niang_Set[4]},  {("nu",2),&nu_Set[2]},        {("nu",3),&nu_Set[3]},        {("nu",4),&nu_Set[4]},        {("nuo",2),&nuo_Set[2]},      {("nuo",3),&nuo_Set[3]},      {("nuo",4),&nuo_Set[4]},      
    {("nuan",2),&nuan_Set[2]},    {("nuan",3),&nuan_Set[3]},    {("nuan",4),&nuan_Set[4]},    {("nun",2),&nun_Set[2]},      {("nun",4),&nun_Set[4]},      {("nv",1),&nv_Set[1]},        {("nv",2),&nv_Set[2]},        
    {("nv",3),&nv_Set[3]},        {("nve",4),&nve_Set[4]},      {("li",0),&li_Set[0]},        {("li",2),&li_Set[2]},        {("li",3),&li_Set[3]},        {("li",4),&li_Set[4]},        {("la",0),&la_Set[0]},        
    {("la",1),&la_Set[1]},        {("la",2),&la_Set[2]},        {("la",3),&la_Set[3]},        {("la",4),&la_Set[4]},        {("lo",0),&lo_Set[0]},        {("le",0),&le_Set[0]},        {("le",1),&le_Set[1]},        
    {("le",4),&le_Set[4]},        {("lai",2),&lai_Set[2]},      {("lai",3),&lai_Set[3]},      {("lai",4),&lai_Set[4]},      {("lei",0),&lei_Set[0]},      {("lei",1),&lei_Set[1]},      {("lei",2),&lei_Set[2]},      
    {("lei",3),&lei_Set[3]},      {("lei",4),&lei_Set[4]},      {("lao",1),&lao_Set[1]},      {("lao",2),&lao_Set[2]},      {("lao",3),&lao_Set[3]},      {("lao",4),&lao_Set[4]},      {("lou",0),&lou_Set[0]},      
    {("lou",1),&lou_Set[1]},      {("lou",2),&lou_Set[2]},      {("lou",3),&lou_Set[3]},      {("lou",4),&lou_Set[4]},      {("lan",2),&lan_Set[2]},      {("lan",3),&lan_Set[3]},      {("lan",4),&lan_Set[4]},      
    {("len",4),&len_Set[4]},      {("lang",1),&lang_Set[1]},    {("lang",2),&lang_Set[2]},    {("lang",3),&lang_Set[3]},    {("lang",4),&lang_Set[4]},    {("leng",1),&leng_Set[1]},    {("leng",2),&leng_Set[2]},    
    {("leng",3),&leng_Set[3]},    {("leng",4),&leng_Set[4]},    {("long",2),&long_Set[2]},    {("long",3),&long_Set[3]},    {("long",4),&long_Set[4]},    {("li",0),&li_Set[0]},        {("li",2),&li_Set[2]},        
    {("li",3),&li_Set[3]},        {("li",4),&li_Set[4]},        {("lia",3),&lia_Set[3]},      {("lie",0),&lie_Set[0]},      {("lie",1),&lie_Set[1]},      {("lie",2),&lie_Set[2]},      {("lie",3),&lie_Set[3]},      
    {("lie",4),&lie_Set[4]},      {("liao",1),&liao_Set[1]},    {("liao",2),&liao_Set[2]},    {("liao",3),&liao_Set[3]},    {("liao",4),&liao_Set[4]},    {("liu",1),&liu_Set[1]},      {("liu",2),&liu_Set[2]},      
    {("liu",3),&liu_Set[3]},      {("liu",4),&liu_Set[4]},      {("lian",2),&lian_Set[2]},    {("lian",3),&lian_Set[3]},    {("lian",4),&lian_Set[4]},    {("lin",2),&lin_Set[2]},      {("lin",3),&lin_Set[3]},      
    {("lin",4),&lin_Set[4]},      {("ling",1),&ling_Set[1]},    {("ling",2),&ling_Set[2]},    {("ling",3),&ling_Set[3]},    {("ling",4),&ling_Set[4]},    {("liang",2),&liang_Set[2]},  {("liang",3),&liang_Set[3]},  
    {("liang",4),&liang_Set[4]},  {("lu",1),&lu_Set[1]},        {("lu",2),&lu_Set[2]},        {("lu",3),&lu_Set[3]},        {("lu",4),&lu_Set[4]},        {("luo",0),&luo_Set[0]},      {("luo",1),&luo_Set[1]},      
    {("luo",2),&luo_Set[2]},      {("luo",3),&luo_Set[3]},      {("luo",4),&luo_Set[4]},      {("luan",2),&luan_Set[2]},    {("luan",3),&luan_Set[3]},    {("luan",4),&luan_Set[4]},    {("lun",1),&lun_Set[1]},      
    {("lun",2),&lun_Set[2]},      {("lun",3),&lun_Set[3]},      {("lun",4),&lun_Set[4]},      {("lv",1),&lv_Set[1]},        {("lv",2),&lv_Set[2]},        {("lv",3),&lv_Set[3]},        {("lve",3),&lve_Set[3]},      
    {("lve",4),&lve_Set[4]},      {("ga",1),&ga_Set[1]},        {("ga",2),&ga_Set[2]},        {("ga",3),&ga_Set[3]},        {("ga",4),&ga_Set[4]},        {("ge",1),&ge_Set[1]},        {("ge",2),&ge_Set[2]},        
    {("ge",3),&ge_Set[3]},        {("ge",4),&ge_Set[4]},        {("gai",1),&gai_Set[1]},      {("gai",3),&gai_Set[3]},      {("gai",4),&gai_Set[4]},      {("gei",3),&gei_Set[3]},      {("gao",1),&gao_Set[1]},      
    {("gao",3),&gao_Set[3]},      {("gao",4),&gao_Set[4]},      {("gou",1),&gou_Set[1]},      {("gou",3),&gou_Set[3]},      {("gou",4),&gou_Set[4]},      {("gan",1),&gan_Set[1]},      {("gan",3),&gan_Set[3]},      
    {("gan",4),&gan_Set[4]},      {("gen",1),&gen_Set[1]},      {("gen",2),&gen_Set[2]},      {("gen",3),&gen_Set[3]},      {("gen",4),&gen_Set[4]},      {("gang",1),&gang_Set[1]},    {("gang",3),&gang_Set[3]},    
    {("gang",4),&gang_Set[4]},    {("geng",1),&geng_Set[1]},    {("geng",3),&geng_Set[3]},    {("geng",4),&geng_Set[4]},    {("gong",1),&gong_Set[1]},    {("gong",3),&gong_Set[3]},    {("gong",4),&gong_Set[4]},    
    {("gu",0),&gu_Set[0]},        {("gu",1),&gu_Set[1]},        {("gu",2),&gu_Set[2]},        {("gu",3),&gu_Set[3]},        {("gu",4),&gu_Set[4]},        {("gua",1),&gua_Set[1]},      {("gua",2),&gua_Set[2]},      
    {("gua",3),&gua_Set[3]},      {("gua",4),&gua_Set[4]},      {("guo",0),&guo_Set[0]},      {("guo",1),&guo_Set[1]},      {("guo",2),&guo_Set[2]},      {("guo",3),&guo_Set[3]},      {("guo",4),&guo_Set[4]},      
    {("guai",1),&guai_Set[1]},    {("guai",3),&guai_Set[3]},    {("guai",4),&guai_Set[4]},    {("gui",1),&gui_Set[1]},      {("gui",3),&gui_Set[3]},      {("gui",4),&gui_Set[4]},      {("guan",1),&guan_Set[1]},    
    {("guan",3),&guan_Set[3]},    {("guan",4),&guan_Set[4]},    {("gun",3),&gun_Set[3]},      {("gun",4),&gun_Set[4]},      {("guang",1),&guang_Set[1]},  {("guang",3),&guang_Set[3]},  {("guang",4),&guang_Set[4]},  
    {("ka",1),&ka_Set[1]},        {("ka",3),&ka_Set[3]},        {("ke",0),&ke_Set[0]},        {("ke",1),&ke_Set[1]},        {("ke",2),&ke_Set[2]},        {("ke",3),&ke_Set[3]},        {("ke",4),&ke_Set[4]},        
    {("kai",1),&kai_Set[1]},      {("kai",3),&kai_Set[3]},      {("kai",4),&kai_Set[4]},      {("kei",1),&kei_Set[1]},      {("kao",1),&kao_Set[1]},      {("kao",3),&kao_Set[3]},      {("kao",4),&kao_Set[4]},      
    {("kou",1),&kou_Set[1]},      {("kou",3),&kou_Set[3]},      {("kou",4),&kou_Set[4]},      {("kan",1),&kan_Set[1]},      {("kan",3),&kan_Set[3]},      {("kan",4),&kan_Set[4]},      {("ken",1),&ken_Set[1]},      
    {("ken",3),&ken_Set[3]},      {("ken",4),&ken_Set[4]},      {("kang",1),&kang_Set[1]},    {("kang",2),&kang_Set[2]},    {("kang",3),&kang_Set[3]},    {("kang",4),&kang_Set[4]},    {("keng",1),&keng_Set[1]},    
    {("keng",3),&keng_Set[3]},    {("kong",1),&kong_Set[1]},    {("kong",3),&kong_Set[3]},    {("kong",4),&kong_Set[4]},    {("ku",1),&ku_Set[1]},        {("ku",2),&ku_Set[2]},        {("ku",3),&ku_Set[3]},        
    {("ku",4),&ku_Set[4]},        {("kua",1),&kua_Set[1]},      {("kua",3),&kua_Set[3]},      {("kua",4),&kua_Set[4]},      {("kuo",4),&kuo_Set[4]},      {("kuai",3),&kuai_Set[3]},    {("kuai",4),&kuai_Set[4]},    
    {("kui",1),&kui_Set[1]},      {("kui",2),&kui_Set[2]},      {("kui",3),&kui_Set[3]},      {("kui",4),&kui_Set[4]},      {("kuan",1),&kuan_Set[1]},    {("kuan",3),&kuan_Set[3]},    {("kun",1),&kun_Set[1]},      
    {("kun",3),&kun_Set[3]},      {("kun",4),&kun_Set[4]},      {("kuang",1),&kuang_Set[1]},  {("kuang",2),&kuang_Set[2]},  {("kuang",3),&kuang_Set[3]},  {("kuang",4),&kuang_Set[4]},  {("ha",1),&ha_Set[1]},        
    {("ha",2),&ha_Set[2]},        {("ha",3),&ha_Set[3]},        {("ha",4),&ha_Set[4]},        {("he",1),&he_Set[1]},        {("he",2),&he_Set[2]},        {("he",3),&he_Set[3]},        {("he",4),&he_Set[4]},        
    {("hai",1),&hai_Set[1]},      {("hai",2),&hai_Set[2]},      {("hai",3),&hai_Set[3]},      {("hai",4),&hai_Set[4]},      {("hei",1),&hei_Set[1]},      {("hao",1),&hao_Set[1]},      {("hao",2),&hao_Set[2]},      
    {("hao",3),&hao_Set[3]},      {("hao",4),&hao_Set[4]},      {("hou",1),&hou_Set[1]},      {("hou",2),&hou_Set[2]},      {("hou",3),&hou_Set[3]},      {("hou",4),&hou_Set[4]},      {("han",1),&han_Set[1]},      
    {("han",2),&han_Set[2]},      {("han",3),&han_Set[3]},      {("han",4),&han_Set[4]},      {("hen",1),&hen_Set[1]},      {("hen",2),&hen_Set[2]},      {("hen",3),&hen_Set[3]},      {("hen",4),&hen_Set[4]},      
    {("hang",1),&hang_Set[1]},    {("hang",2),&hang_Set[2]},    {("hang",3),&hang_Set[3]},    {("hang",4),&hang_Set[4]},    {("heng",1),&heng_Set[1]},    {("heng",2),&heng_Set[2]},    {("heng",4),&heng_Set[4]},    
    {("hong",1),&hong_Set[1]},    {("hong",2),&hong_Set[2]},    {("hong",3),&hong_Set[3]},    {("hong",4),&hong_Set[4]},    {("hu",1),&hu_Set[1]},        {("hu",2),&hu_Set[2]},        {("hu",3),&hu_Set[3]},        
    {("hu",4),&hu_Set[4]},        {("hua",1),&hua_Set[1]},      {("hua",2),&hua_Set[2]},      {("hua",4),&hua_Set[4]},      {("huo",0),&huo_Set[0]},      {("huo",1),&huo_Set[1]},      {("huo",2),&huo_Set[2]},      
    {("huo",3),&huo_Set[3]},      {("huo",4),&huo_Set[4]},      {("huai",0),&huai_Set[0]},    {("huai",2),&huai_Set[2]},    {("huai",4),&huai_Set[4]},    {("hui",0),&hui_Set[0]},      {("hui",1),&hui_Set[1]},      
    {("hui",2),&hui_Set[2]},      {("hui",3),&hui_Set[3]},      {("hui",4),&hui_Set[4]},      {("huan",1),&huan_Set[1]},    {("huan",2),&huan_Set[2]},    {("huan",3),&huan_Set[3]},    {("huan",4),&huan_Set[4]},    
    {("hun",1),&hun_Set[1]},      {("hun",2),&hun_Set[2]},      {("hun",3),&hun_Set[3]},      {("hun",4),&hun_Set[4]},      {("huang",0),&huang_Set[0]},  {("huang",1),&huang_Set[1]},  {("huang",2),&huang_Set[2]},  
    {("huang",3),&huang_Set[3]},  {("huang",4),&huang_Set[4]},  {("ji",1),&ji_Set[1]},        {("ji",2),&ji_Set[2]},        {("ji",3),&ji_Set[3]},        {("ji",4),&ji_Set[4]},        {("ji",1),&ji_Set[1]},        
    {("ji",2),&ji_Set[2]},        {("ji",3),&ji_Set[3]},        {("ji",4),&ji_Set[4]},        {("jia",0),&jia_Set[0]},      {("jia",1),&jia_Set[1]},      {("jia",2),&jia_Set[2]},      {("jia",3),&jia_Set[3]},      
    {("jia",4),&jia_Set[4]},      {("jie",0),&jie_Set[0]},      {("jie",1),&jie_Set[1]},      {("jie",2),&jie_Set[2]},      {("jie",3),&jie_Set[3]},      {("jie",4),&jie_Set[4]},      {("jiao",1),&jiao_Set[1]},    
    {("jiao",2),&jiao_Set[2]},    {("jiao",3),&jiao_Set[3]},    {("jiao",4),&jiao_Set[4]},    {("jiu",1),&jiu_Set[1]},      {("jiu",3),&jiu_Set[3]},      {("jiu",4),&jiu_Set[4]},      {("jian",1),&jian_Set[1]},    
    {("jian",3),&jian_Set[3]},    {("jian",4),&jian_Set[4]},    {("jin",1),&jin_Set[1]},      {("jin",3),&jin_Set[3]},      {("jin",4),&jin_Set[4]},      {("jing",1),&jing_Set[1]},    {("jing",3),&jing_Set[3]},    
    {("jing",4),&jing_Set[4]},    {("jiang",1),&jiang_Set[1]},  {("jiang",3),&jiang_Set[3]},  {("jiang",4),&jiang_Set[4]},  {("jiong",1),&jiong_Set[1]},  {("jiong",3),&jiong_Set[3]},  {("jiong",4),&jiong_Set[4]},  
    {("ju",1),&ju_Set[1]},        {("ju",2),&ju_Set[2]},        {("ju",3),&ju_Set[3]},        {("ju",4),&ju_Set[4]},        {("juan",1),&juan_Set[1]},    {("juan",3),&juan_Set[3]},    {("juan",4),&juan_Set[4]},    
    {("jun",1),&jun_Set[1]},      {("jun",3),&jun_Set[3]},      {("jun",4),&jun_Set[4]},      {("qi",1),&qi_Set[1]},        {("qi",2),&qi_Set[2]},        {("qi",3),&qi_Set[3]},        {("qi",4),&qi_Set[4]},        
    {("qi",1),&qi_Set[1]},        {("qi",2),&qi_Set[2]},        {("qi",3),&qi_Set[3]},        {("qi",4),&qi_Set[4]},        {("qia",1),&qia_Set[1]},      {("qia",2),&qia_Set[2]},      {("qia",3),&qia_Set[3]},      
    {("qia",4),&qia_Set[4]},      {("qie",1),&qie_Set[1]},      {("qie",2),&qie_Set[2]},      {("qie",3),&qie_Set[3]},      {("qie",4),&qie_Set[4]},      {("qiao",1),&qiao_Set[1]},    {("qiao",2),&qiao_Set[2]},    
    {("qiao",3),&qiao_Set[3]},    {("qiao",4),&qiao_Set[4]},    {("qiu",1),&qiu_Set[1]},      {("qiu",2),&qiu_Set[2]},      {("qiu",3),&qiu_Set[3]},      {("qiu",4),&qiu_Set[4]},      {("qian",1),&qian_Set[1]},    
    {("qian",2),&qian_Set[2]},    {("qian",3),&qian_Set[3]},    {("qian",4),&qian_Set[4]},    {("qin",1),&qin_Set[1]},      {("qin",2),&qin_Set[2]},      {("qin",3),&qin_Set[3]},      {("qin",4),&qin_Set[4]},      
    {("qing",1),&qing_Set[1]},    {("qing",2),&qing_Set[2]},    {("qing",3),&qing_Set[3]},    {("qing",4),&qing_Set[4]},    {("qiang",1),&qiang_Set[1]},  {("qiang",2),&qiang_Set[2]},  {("qiang",3),&qiang_Set[3]},  
    {("qiang",4),&qiang_Set[4]},  {("qiong",1),&qiong_Set[1]},  {("qiong",2),&qiong_Set[2]},  {("qiong",4),&qiong_Set[4]},  {("qu",0),&qu_Set[0]},        {("qu",1),&qu_Set[1]},        {("qu",2),&qu_Set[2]},        
    {("qu",3),&qu_Set[3]},        {("qu",4),&qu_Set[4]},        {("quan",1),&quan_Set[1]},    {("quan",2),&quan_Set[2]},    {("quan",3),&quan_Set[3]},    {("quan",4),&quan_Set[4]},    {("qun",1),&qun_Set[1]},      
    {("qun",2),&qun_Set[2]},      {("qun",3),&qun_Set[3]},      {("xi",1),&xi_Set[1]},        {("xi",2),&xi_Set[2]},        {("xi",3),&xi_Set[3]},        {("xi",4),&xi_Set[4]},        {("xi",1),&xi_Set[1]},        
    {("xi",2),&xi_Set[2]},        {("xi",3),&xi_Set[3]},        {("xi",4),&xi_Set[4]},        {("xia",1),&xia_Set[1]},      {("xia",2),&xia_Set[2]},      {("xia",3),&xia_Set[3]},      {("xia",4),&xia_Set[4]},      
    {("xie",1),&xie_Set[1]},      {("xie",2),&xie_Set[2]},      {("xie",3),&xie_Set[3]},      {("xie",4),&xie_Set[4]},      {("xiao",1),&xiao_Set[1]},    {("xiao",2),&xiao_Set[2]},    {("xiao",3),&xiao_Set[3]},    
    {("xiao",4),&xiao_Set[4]},    {("xiu",1),&xiu_Set[1]},      {("xiu",2),&xiu_Set[2]},      {("xiu",3),&xiu_Set[3]},      {("xiu",4),&xiu_Set[4]},      {("xian",1),&xian_Set[1]},    {("xian",2),&xian_Set[2]},    
    {("xian",3),&xian_Set[3]},    {("xian",4),&xian_Set[4]},    {("xin",1),&xin_Set[1]},      {("xin",2),&xin_Set[2]},      {("xin",3),&xin_Set[3]},      {("xin",4),&xin_Set[4]},      {("xing",1),&xing_Set[1]},    
    {("xing",2),&xing_Set[2]},    {("xing",3),&xing_Set[3]},    {("xing",4),&xing_Set[4]},    {("xiang",1),&xiang_Set[1]},  {("xiang",2),&xiang_Set[2]},  {("xiang",3),&xiang_Set[3]},  {("xiang",4),&xiang_Set[4]},  
    {("xiong",1),&xiong_Set[1]},  {("xiong",2),&xiong_Set[2]},  {("xiong",3),&xiong_Set[3]},  {("xiong",4),&xiong_Set[4]},  {("xu",0),&xu_Set[0]},        {("xu",1),&xu_Set[1]},        {("xu",2),&xu_Set[2]},        
    {("xu",3),&xu_Set[3]},        {("xu",4),&xu_Set[4]},        {("xuan",1),&xuan_Set[1]},    {("xuan",2),&xuan_Set[2]},    {("xuan",3),&xuan_Set[3]},    {("xuan",4),&xuan_Set[4]},    {("xun",1),&xun_Set[1]},      
    {("xun",2),&xun_Set[2]},      {("xun",4),&xun_Set[4]},      {("zhi",1),&zhi_Set[1]},      {("zhi",2),&zhi_Set[2]},      {("zhi",3),&zhi_Set[3]},      {("zhi",4),&zhi_Set[4]},      {("zha",0),&zha_Set[0]},      
    {("zha",1),&zha_Set[1]},      {("zha",2),&zha_Set[2]},      {("zha",3),&zha_Set[3]},      {("zha",4),&zha_Set[4]},      {("zhe",0),&zhe_Set[0]},      {("zhe",1),&zhe_Set[1]},      {("zhe",2),&zhe_Set[2]},      
    {("zhe",3),&zhe_Set[3]},      {("zhe",4),&zhe_Set[4]},      {("zhai",1),&zhai_Set[1]},    {("zhai",2),&zhai_Set[2]},    {("zhai",3),&zhai_Set[3]},    {("zhai",4),&zhai_Set[4]},    {("zhei",4),&zhei_Set[4]},    
    {("zhao",1),&zhao_Set[1]},    {("zhao",2),&zhao_Set[2]},    {("zhao",3),&zhao_Set[3]},    {("zhao",4),&zhao_Set[4]},    {("zhou",1),&zhou_Set[1]},    {("zhou",2),&zhou_Set[2]},    {("zhou",3),&zhou_Set[3]},    
    {("zhou",4),&zhou_Set[4]},    {("zhan",1),&zhan_Set[1]},    {("zhan",2),&zhan_Set[2]},    {("zhan",3),&zhan_Set[3]},    {("zhan",4),&zhan_Set[4]},    {("zhen",1),&zhen_Set[1]},    {("zhen",2),&zhen_Set[2]},    
    {("zhen",3),&zhen_Set[3]},    {("zhen",4),&zhen_Set[4]},    {("zhang",1),&zhang_Set[1]},  {("zhang",3),&zhang_Set[3]},  {("zhang",4),&zhang_Set[4]},  {("zheng",1),&zheng_Set[1]},  {("zheng",3),&zheng_Set[3]},  
    {("zheng",4),&zheng_Set[4]},  {("zhong",1),&zhong_Set[1]},  {("zhong",3),&zhong_Set[3]},  {("zhong",4),&zhong_Set[4]},  {("zhi",1),&zhi_Set[1]},      {("zhi",2),&zhi_Set[2]},      {("zhi",3),&zhi_Set[3]},      
    {("zhi",4),&zhi_Set[4]},      {("zhu",1),&zhu_Set[1]},      {("zhu",2),&zhu_Set[2]},      {("zhu",3),&zhu_Set[3]},      {("zhu",4),&zhu_Set[4]},      {("zhua",1),&zhua_Set[1]},    {("zhua",3),&zhua_Set[3]},    
    {("zhuo",1),&zhuo_Set[1]},    {("zhuo",2),&zhuo_Set[2]},    {("zhuo",4),&zhuo_Set[4]},    {("zhuai",1),&zhuai_Set[1]},  {("zhuai",3),&zhuai_Set[3]},  {("zhuai",4),&zhuai_Set[4]},  {("zhui",1),&zhui_Set[1]},    
    {("zhui",3),&zhui_Set[3]},    {("zhui",4),&zhui_Set[4]},    {("zhuan",1),&zhuan_Set[1]},  {("zhuan",3),&zhuan_Set[3]},  {("zhuan",4),&zhuan_Set[4]},  {("zhun",1),&zhun_Set[1]},    {("zhun",3),&zhun_Set[3]},    
    {("zhun",4),&zhun_Set[4]},    {("zhuang",1),&zhuang_Set[1]},{("zhuang",3),&zhuang_Set[3]},{("zhuang",4),&zhuang_Set[4]},{("chi",1),&chi_Set[1]},      {("chi",2),&chi_Set[2]},      {("chi",3),&chi_Set[3]},      
    {("chi",4),&chi_Set[4]},      {("cha",1),&cha_Set[1]},      {("cha",2),&cha_Set[2]},      {("cha",3),&cha_Set[3]},      {("cha",4),&cha_Set[4]},      {("che",1),&che_Set[1]},      {("che",2),&che_Set[2]},      
    {("che",3),&che_Set[3]},      {("che",4),&che_Set[4]},      {("chai",1),&chai_Set[1]},    {("chai",2),&chai_Set[2]},    {("chai",3),&chai_Set[3]},    {("chai",4),&chai_Set[4]},    {("chao",1),&chao_Set[1]},    
    {("chao",2),&chao_Set[2]},    {("chao",3),&chao_Set[3]},    {("chao",4),&chao_Set[4]},    {("chou",1),&chou_Set[1]},    {("chou",2),&chou_Set[2]},    {("chou",3),&chou_Set[3]},    {("chou",4),&chou_Set[4]},    
    {("chan",1),&chan_Set[1]},    {("chan",2),&chan_Set[2]},    {("chan",3),&chan_Set[3]},    {("chan",4),&chan_Set[4]},    {("chen",0),&chen_Set[0]},    {("chen",1),&chen_Set[1]},    {("chen",2),&chen_Set[2]},    
    {("chen",3),&chen_Set[3]},    {("chen",4),&chen_Set[4]},    {("chang",1),&chang_Set[1]},  {("chang",2),&chang_Set[2]},  {("chang",3),&chang_Set[3]},  {("chang",4),&chang_Set[4]},  {("cheng",1),&cheng_Set[1]},  
    {("cheng",2),&cheng_Set[2]},  {("cheng",3),&cheng_Set[3]},  {("cheng",4),&cheng_Set[4]},  {("chong",1),&chong_Set[1]},  {("chong",2),&chong_Set[2]},  {("chong",3),&chong_Set[3]},  {("chong",4),&chong_Set[4]},  
    {("chi",1),&chi_Set[1]},      {("chi",2),&chi_Set[2]},      {("chi",3),&chi_Set[3]},      {("chi",4),&chi_Set[4]},      {("chu",1),&chu_Set[1]},      {("chu",2),&chu_Set[2]},      {("chu",3),&chu_Set[3]},      
    {("chu",4),&chu_Set[4]},      {("chua",1),&chua_Set[1]},    {("chua",3),&chua_Set[3]},    {("chua",4),&chua_Set[4]},    {("chuo",1),&chuo_Set[1]},    {("chuo",4),&chuo_Set[4]},    {("chuai",1),&chuai_Set[1]},  
    {("chuai",2),&chuai_Set[2]},  {("chuai",3),&chuai_Set[3]},  {("chuai",4),&chuai_Set[4]},  {("chui",1),&chui_Set[1]},    {("chui",2),&chui_Set[2]},    {("chui",3),&chui_Set[3]},    {("chui",4),&chui_Set[4]},    
    {("chuan",1),&chuan_Set[1]},  {("chuan",2),&chuan_Set[2]},  {("chuan",3),&chuan_Set[3]},  {("chuan",4),&chuan_Set[4]},  {("chun",1),&chun_Set[1]},    {("chun",2),&chun_Set[2]},    {("chun",3),&chun_Set[3]},    
    {("chuang",1),&chuang_Set[1]},{("chuang",2),&chuang_Set[2]},{("chuang",3),&chuang_Set[3]},{("chuang",4),&chuang_Set[4]},{("shi",0),&shi_Set[0]},      {("shi",1),&shi_Set[1]},      {("shi",2),&shi_Set[2]},      
    {("shi",3),&shi_Set[3]},      {("shi",4),&shi_Set[4]},      {("sha",1),&sha_Set[1]},      {("sha",3),&sha_Set[3]},      {("sha",4),&sha_Set[4]},      {("she",1),&she_Set[1]},      {("she",2),&she_Set[2]},      
    {("she",3),&she_Set[3]},      {("she",4),&she_Set[4]},      {("shai",1),&shai_Set[1]},    {("shai",3),&shai_Set[3]},    {("shai",4),&shai_Set[4]},    {("shei",2),&shei_Set[2]},    {("shao",1),&shao_Set[1]},    
    {("shao",2),&shao_Set[2]},    {("shao",3),&shao_Set[3]},    {("shao",4),&shao_Set[4]},    {("shou",1),&shou_Set[1]},    {("shou",2),&shou_Set[2]},    {("shou",3),&shou_Set[3]},    {("shou",4),&shou_Set[4]},    
    {("shan",1),&shan_Set[1]},    {("shan",2),&shan_Set[2]},    {("shan",3),&shan_Set[3]},    {("shan",4),&shan_Set[4]},    {("shen",1),&shen_Set[1]},    {("shen",2),&shen_Set[2]},    {("shen",3),&shen_Set[3]},    
    {("shen",4),&shen_Set[4]},    {("shang",1),&shang_Set[1]},  {("shang",3),&shang_Set[3]},  {("shang",4),&shang_Set[4]},  {("sheng",1),&sheng_Set[1]},  {("sheng",2),&sheng_Set[2]},  {("sheng",3),&sheng_Set[3]},  
    {("sheng",4),&sheng_Set[4]},  {("shi",0),&shi_Set[0]},      {("shi",1),&shi_Set[1]},      {("shi",2),&shi_Set[2]},      {("shi",3),&shi_Set[3]},      {("shi",4),&shi_Set[4]},      {("shu",1),&shu_Set[1]},      
    {("shu",2),&shu_Set[2]},      {("shu",3),&shu_Set[3]},      {("shu",4),&shu_Set[4]},      {("shua",1),&shua_Set[1]},    {("shua",3),&shua_Set[3]},    {("shua",4),&shua_Set[4]},    {("shuo",1),&shuo_Set[1]},    
    {("shuo",2),&shuo_Set[2]},    {("shuo",4),&shuo_Set[4]},    {("shuai",1),&shuai_Set[1]},  {("shuai",3),&shuai_Set[3]},  {("shuai",4),&shuai_Set[4]},  {("shui",2),&shui_Set[2]},    {("shui",3),&shui_Set[3]},    
    {("shui",4),&shui_Set[4]},    {("shuan",1),&shuan_Set[1]},  {("shuan",4),&shuan_Set[4]},  {("shun",3),&shun_Set[3]},    {("shun",4),&shun_Set[4]},    {("shuang",1),&shuang_Set[1]},{("shuang",3),&shuang_Set[3]},
    {("shuang",4),&shuang_Set[4]},{("ri",4),&ri_Set[4]},        {("re",2),&re_Set[2]},        {("re",3),&re_Set[3]},        {("re",4),&re_Set[4]},        {("rao",2),&rao_Set[2]},      {("rao",3),&rao_Set[3]},      
    {("rao",4),&rao_Set[4]},      {("rou",2),&rou_Set[2]},      {("rou",3),&rou_Set[3]},      {("rou",4),&rou_Set[4]},      {("ran",2),&ran_Set[2]},      {("ran",3),&ran_Set[3]},      {("ran",4),&ran_Set[4]},      
    {("ren",2),&ren_Set[2]},      {("ren",3),&ren_Set[3]},      {("ren",4),&ren_Set[4]},      {("rang",1),&rang_Set[1]},    {("rang",2),&rang_Set[2]},    {("rang",3),&rang_Set[3]},    {("rang",4),&rang_Set[4]},    
    {("reng",1),&reng_Set[1]},    {("reng",2),&reng_Set[2]},    {("reng",4),&reng_Set[4]},    {("rong",2),&rong_Set[2]},    {("rong",3),&rong_Set[3]},    {("rong",4),&rong_Set[4]},    {("ri",4),&ri_Set[4]},        
    {("ru",2),&ru_Set[2]},        {("ru",3),&ru_Set[3]},        {("ru",4),&ru_Set[4]},        {("rua",2),&rua_Set[2]},      {("ruo",2),&ruo_Set[2]},      {("ruo",4),&ruo_Set[4]},      {("rui",2),&rui_Set[2]},      
    {("rui",3),&rui_Set[3]},      {("rui",4),&rui_Set[4]},      {("ruan",2),&ruan_Set[2]},    {("ruan",3),&ruan_Set[3]},    {("ruan",4),&ruan_Set[4]},    {("run",2),&run_Set[2]},      {("run",3),&run_Set[3]},      
    {("run",4),&run_Set[4]},      {("zi",0),&zi_Set[0]},        {("zi",1),&zi_Set[1]},        {("zi",2),&zi_Set[2]},        {("zi",3),&zi_Set[3]},        {("zi",4),&zi_Set[4]},        {("za",1),&za_Set[1]},        
    {("za",2),&za_Set[2]},        {("za",3),&za_Set[3]},        {("za",4),&za_Set[4]},        {("ze",2),&ze_Set[2]},        {("ze",4),&ze_Set[4]},        {("zai",1),&zai_Set[1]},      {("zai",3),&zai_Set[3]},      
    {("zai",4),&zai_Set[4]},      {("zei",2),&zei_Set[2]},      {("zao",1),&zao_Set[1]},      {("zao",2),&zao_Set[2]},      {("zao",3),&zao_Set[3]},      {("zao",4),&zao_Set[4]},      {("zou",1),&zou_Set[1]},      
    {("zou",3),&zou_Set[3]},      {("zou",4),&zou_Set[4]},      {("zan",0),&zan_Set[0]},      {("zan",1),&zan_Set[1]},      {("zan",2),&zan_Set[2]},      {("zan",3),&zan_Set[3]},      {("zan",4),&zan_Set[4]},      
    {("zen",1),&zen_Set[1]},      {("zen",3),&zen_Set[3]},      {("zen",4),&zen_Set[4]},      {("zang",1),&zang_Set[1]},    {("zang",3),&zang_Set[3]},    {("zang",4),&zang_Set[4]},    {("zeng",1),&zeng_Set[1]},    
    {("zeng",3),&zeng_Set[3]},    {("zeng",4),&zeng_Set[4]},    {("zong",1),&zong_Set[1]},    {("zong",3),&zong_Set[3]},    {("zong",4),&zong_Set[4]},    {("zi",0),&zi_Set[0]},        {("zi",1),&zi_Set[1]},        
    {("zi",2),&zi_Set[2]},        {("zi",3),&zi_Set[3]},        {("zi",4),&zi_Set[4]},        {("zu",1),&zu_Set[1]},        {("zu",2),&zu_Set[2]},        {("zu",3),&zu_Set[3]},        {("zu",4),&zu_Set[4]},        
    {("zuo",0),&zuo_Set[0]},      {("zuo",1),&zuo_Set[1]},      {("zuo",2),&zuo_Set[2]},      {("zuo",3),&zuo_Set[3]},      {("zuo",4),&zuo_Set[4]},      {("zui",1),&zui_Set[1]},      {("zui",2),&zui_Set[2]},      
    {("zui",3),&zui_Set[3]},      {("zui",4),&zui_Set[4]},      {("zuan",1),&zuan_Set[1]},    {("zuan",3),&zuan_Set[3]},    {("zuan",4),&zuan_Set[4]},    {("zun",1),&zun_Set[1]},      {("zun",2),&zun_Set[2]},      
    {("zun",3),&zun_Set[3]},      {("zun",4),&zun_Set[4]},      {("ci",1),&ci_Set[1]},        {("ci",2),&ci_Set[2]},        {("ci",3),&ci_Set[3]},        {("ci",4),&ci_Set[4]},        {("ca",1),&ca_Set[1]},        
    {("ca",3),&ca_Set[3]},        {("ca",4),&ca_Set[4]},        {("ce",4),&ce_Set[4]},        {("cai",1),&cai_Set[1]},      {("cai",2),&cai_Set[2]},      {("cai",3),&cai_Set[3]},      {("cai",4),&cai_Set[4]},      
    {("cao",1),&cao_Set[1]},      {("cao",2),&cao_Set[2]},      {("cao",3),&cao_Set[3]},      {("cao",4),&cao_Set[4]},      {("cou",1),&cou_Set[1]},      {("cou",2),&cou_Set[2]},      {("cou",3),&cou_Set[3]},      
    {("cou",4),&cou_Set[4]},      {("can",1),&can_Set[1]},      {("can",2),&can_Set[2]},      {("can",3),&can_Set[3]},      {("can",4),&can_Set[4]},      {("cen",1),&cen_Set[1]},      {("cen",2),&cen_Set[2]},      
    {("cang",1),&cang_Set[1]},    {("cang",2),&cang_Set[2]},    {("cang",3),&cang_Set[3]},    {("cang",4),&cang_Set[4]},    {("ceng",1),&ceng_Set[1]},    {("ceng",2),&ceng_Set[2]},    {("ceng",4),&ceng_Set[4]},    
    {("cong",1),&cong_Set[1]},    {("cong",2),&cong_Set[2]},    {("cong",3),&cong_Set[3]},    {("cong",4),&cong_Set[4]},    {("ci",1),&ci_Set[1]},        {("ci",2),&ci_Set[2]},        {("ci",3),&ci_Set[3]},        
    {("ci",4),&ci_Set[4]},        {("cu",1),&cu_Set[1]},        {("cu",2),&cu_Set[2]},        {("cu",3),&cu_Set[3]},        {("cu",4),&cu_Set[4]},        {("cuo",1),&cuo_Set[1]},      {("cuo",2),&cuo_Set[2]},      
    {("cuo",3),&cuo_Set[3]},      {("cuo",4),&cuo_Set[4]},      {("cui",1),&cui_Set[1]},      {("cui",3),&cui_Set[3]},      {("cui",4),&cui_Set[4]},      {("cuan",1),&cuan_Set[1]},    {("cuan",2),&cuan_Set[2]},    
    {("cuan",4),&cuan_Set[4]},    {("cun",1),&cun_Set[1]},      {("cun",2),&cun_Set[2]},      {("cun",3),&cun_Set[3]},      {("cun",4),&cun_Set[4]},      {("si",0),&si_Set[0]},        {("si",1),&si_Set[1]},        
    {("si",2),&si_Set[2]},        {("si",3),&si_Set[3]},        {("si",4),&si_Set[4]},        {("sa",0),&sa_Set[0]},        {("sa",1),&sa_Set[1]},        {("sa",3),&sa_Set[3]},        {("sa",4),&sa_Set[4]},        
    {("se",4),&se_Set[4]},        {("sai",1),&sai_Set[1]},      {("sai",3),&sai_Set[3]},      {("sai",4),&sai_Set[4]},      {("sao",1),&sao_Set[1]},      {("sao",3),&sao_Set[3]},      {("sao",4),&sao_Set[4]},      
    {("sou",1),&sou_Set[1]},      {("sou",3),&sou_Set[3]},      {("sou",4),&sou_Set[4]},      {("san",0),&san_Set[0]},      {("san",1),&san_Set[1]},      {("san",3),&san_Set[3]},      {("san",4),&san_Set[4]},      
    {("sen",1),&sen_Set[1]},      {("sen",3),&sen_Set[3]},      {("sang",1),&sang_Set[1]},    {("sang",3),&sang_Set[3]},    {("sang",4),&sang_Set[4]},    {("seng",1),&seng_Set[1]},    {("seng",4),&seng_Set[4]},    
    {("song",1),&song_Set[1]},    {("song",2),&song_Set[2]},    {("song",3),&song_Set[3]},    {("song",4),&song_Set[4]},    {("si",0),&si_Set[0]},        {("si",1),&si_Set[1]},        {("si",2),&si_Set[2]},        
    {("si",3),&si_Set[3]},        {("si",4),&si_Set[4]},        {("su",1),&su_Set[1]},        {("su",2),&su_Set[2]},        {("su",3),&su_Set[3]},        {("su",4),&su_Set[4]},        {("suo",1),&suo_Set[1]},      
    {("suo",2),&suo_Set[2]},      {("suo",3),&suo_Set[3]},      {("suo",4),&suo_Set[4]},      {("sui",1),&sui_Set[1]},      {("sui",2),&sui_Set[2]},      {("sui",3),&sui_Set[3]},      {("sui",4),&sui_Set[4]},      
    {("suan",1),&suan_Set[1]},    {("suan",3),&suan_Set[3]},    {("suan",4),&suan_Set[4]},    {("sun",1),&sun_Set[1]},      {("sun",3),&sun_Set[3]},      {("sun",4),&sun_Set[4]},      {("yi",1),&yi_Set[1]},        
    {("yi",2),&yi_Set[2]},        {("yi",3),&yi_Set[3]},        {("yi",4),&yi_Set[4]},        {("ya",0),&ya_Set[0]},        {("ya",1),&ya_Set[1]},        {("ya",2),&ya_Set[2]},        {("ya",3),&ya_Set[3]},        
    {("ya",4),&ya_Set[4]},        {("yo",0),&yo_Set[0]},        {("yo",1),&yo_Set[1]},        {("ye",1),&ye_Set[1]},        {("ye",2),&ye_Set[2]},        {("ye",3),&ye_Set[3]},        {("ye",4),&ye_Set[4]},        
    {("yao",1),&yao_Set[1]},      {("yao",2),&yao_Set[2]},      {("yao",3),&yao_Set[3]},      {("yao",4),&yao_Set[4]},      {("you",1),&you_Set[1]},      {("you",2),&you_Set[2]},      {("you",3),&you_Set[3]},      
    {("you",4),&you_Set[4]},      {("yan",1),&yan_Set[1]},      {("yan",2),&yan_Set[2]},      {("yan",3),&yan_Set[3]},      {("yan",4),&yan_Set[4]},      {("yang",1),&yang_Set[1]},    {("yang",2),&yang_Set[2]},    
    {("yang",3),&yang_Set[3]},    {("yang",4),&yang_Set[4]},    {("yong",1),&yong_Set[1]},    {("yong",2),&yong_Set[2]},    {("yong",3),&yong_Set[3]},    {("yong",4),&yong_Set[4]},    {("yi",1),&yi_Set[1]},        
    {("yi",2),&yi_Set[2]},        {("yi",3),&yi_Set[3]},        {("yi",4),&yi_Set[4]},        {("yin",1),&yin_Set[1]},      {("yin",2),&yin_Set[2]},      {("yin",3),&yin_Set[3]},      {("yin",4),&yin_Set[4]},      
    {("ying",1),&ying_Set[1]},    {("ying",2),&ying_Set[2]},    {("ying",3),&ying_Set[3]},    {("ying",4),&ying_Set[4]},    {("yu",1),&yu_Set[1]},        {("yu",2),&yu_Set[2]},        {("yu",3),&yu_Set[3]},        
    {("yu",4),&yu_Set[4]},        {("yuan",1),&yuan_Set[1]},    {("yuan",2),&yuan_Set[2]},    {("yuan",3),&yuan_Set[3]},    {("yuan",4),&yuan_Set[4]},    {("yun",1),&yun_Set[1]},      {("yun",2),&yun_Set[2]},      
    {("yun",3),&yun_Set[3]},      {("yun",4),&yun_Set[4]},      {("wa",0),&wa_Set[0]},        {("wa",1),&wa_Set[1]},        {("wa",2),&wa_Set[2]},        {("wa",3),&wa_Set[3]},        {("wa",4),&wa_Set[4]},        
    {("wo",1),&wo_Set[1]},        {("wo",3),&wo_Set[3]},        {("wo",4),&wo_Set[4]},        {("wai",0),&wai_Set[0]},      {("wai",1),&wai_Set[1]},      {("wai",3),&wai_Set[3]},      {("wai",4),&wai_Set[4]},      
    {("wei",1),&wei_Set[1]},      {("wei",2),&wei_Set[2]},      {("wei",3),&wei_Set[3]},      {("wei",4),&wei_Set[4]},      {("wan",1),&wan_Set[1]},      {("wan",2),&wan_Set[2]},      {("wan",3),&wan_Set[3]},      
    {("wan",4),&wan_Set[4]},      {("wen",1),&wen_Set[1]},      {("wen",2),&wen_Set[2]},      {("wen",3),&wen_Set[3]},      {("wen",4),&wen_Set[4]},      {("wang",1),&wang_Set[1]},    {("wang",2),&wang_Set[2]},    
    {("wang",3),&wang_Set[3]},    {("wang",4),&wang_Set[4]},    {("weng",1),&weng_Set[1]},    {("weng",3),&weng_Set[3]},    {("weng",4),&weng_Set[4]},    {("wong",4),&wong_Set[4]},    {("wu",1),&wu_Set[1]},        
    {("wu",2),&wu_Set[2]},        {("wu",3),&wu_Set[3]},        {("wu",4),&wu_Set[4]}
    };
