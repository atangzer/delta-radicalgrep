# README-radicalgrep

Radical Grep is a tool built off of the icgrep engine. It searches for the given Chinese radicals, and returns the phrase(s) that correspond with the input. Note that radicals will be processed according to the Kangxi Radical-Stroke indices.

For more information on the Kangxi Radical System, please visit: https://en.wikipedia.org/wiki/Kangxi_radical or https://www.yellowbridge.com/chinese/radicals.php

## **Installation**

To build radical grep, the working environment needs to have all requirements of the icgrep build met. icgrep must also be built beforehand.

After the above has been done, run  `make`  and `make check` on the terminal to build the software and run the test suite.

To build only Radical Grep and it's dependencies, run `make radicalgrep`.

After everything passes, you are ready to run Radical Grep.

## **How to Run Radical Grep**

To run Radical Grep, run the following commands in the bin directory:

    ./radicalgrep <Radical Expression> <Path of Input File>

## **Iteration 1: Hard-Coding the Testcases into the Program**

In the first iteration, Radical Grep takes in pre-programmed inputs and returns the phrase with the corresponding radicals.

 ## Example 1

    Input:  亻_心_ ../QA/radicaltest/testfiles/test1
    Output: 以下是一些关于部首分类的信息

## Example 2

    Input:  氵_宀 _ ../QA/radicaltest/testfiles/test1
    Output: 这是采用“两分法”对汉字进行结构分析得出的认识


## **Iteration 2: Radical Count & Grep Implementation**

In the second iteration, Radical Grep takes actual Kangxi radical(s) as input (e.g. "子_" or " 氵_子 _"). It returns the sentence with the correspondings radicals marked in red text. Iteration 2 of Radical Grep can be run using the same input format as iteration 1.

Another program, `Radical Count` was implemented in this iteration. The program and relevant documentation can be found in `parabix-devel/tools/wc/radical_count`.

## Iteration 2 Milestones

### Radical Grep Version 2.1.0 
* Radical Grep takes the Kangxi radical indices (e.g. "85_85_" as 氵_氵_) as input.
### Radical Grep Version 2.1.1
* Output added for the case where no match is found.
### Radical Grep Version 2.2.0
* Functionality to perform the search using the actual radicals (e.g. "氵_氵_") and not the Kangxi indices is added. 
### Radical Grep Version 2.2.1 
* Functionality for single radicals (e.g. "氵_") is now supported.

## Example 1

    Input: 子_ ../QA/radicaltest/testfiles/test1
    Output: 这是一个简单的例**子**
    部首分类也是使用汉**字**之文化圈少数的共通点
    部首检字也有其局限性，许多汉**字**难以归部
    
## Example 2

    Input: 氵_子_ ../QA/radicaltest/testfiles/test1
    Output: 部首分类也是使用**汉字**之文化圈少数的共通点
    部首检字也有其局限性，许多**汉字**难以归部
   
## Example 3

    Input: 子_子_ ../QA/radicaltest/testfiles/test1
    Output: Can not find the results!

###### ** Output is printed in red on the terminal. ** 

## **Iteration 3: Adding New Features**
Plans for iteration 3 include:

1. Implement switch between two search modes, users can choose any search mode; kangxi radical indices and actual kangxi radical.
2. Add more functions/command line flags.


**Authored by Team Delta:** Anna Tang, Lexie Yu (Yu Ruo Nan),  Pan Chu Wen

**Last Updated:** 2020/05/15
