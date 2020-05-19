# Implementation Guide-Radical Count

This document provides an overview of the Radical Count implementation. Please review [README-radicalcount](https://cs-git-research.cs.surrey.sfu.ca/cameron/parabix-devel/blob/delta-radicalgrep/tools/wc/radical_count/README-radicalcount.md) before reading on.

This guide only provides a simple explaination of the key concepts and data structures used. Please refer to the source code for more details and inline documentation as well.

## **File Directory**
To find the source code for Radical Count, go into `tools/wc/radical_count`. The radical_count folder contains all the necessary files to build the program.

```
radical_count
├── CMakeLists.txt
├── README-radicalcount.md    
│   
└───radicalScripts
│   ├── create_table.py
│   ├── radical_table.txt
│   ├── unicodeset_radical_table.txt
│   
├── radicalcount.cpp
├── radical_interface.cpp
├── radical_interface.h
```
The `radicalScripts` folder contains scripts that were used to generate the unicode sets that were used in `radical_interface.cpp` for both Radical Grep and Radical Count. It references all 214 Kangxi radicals, obtained from [kRSKangXi.h](https://cs-git-research.cs.surrey.sfu.ca/cameron/parabix-devel/blob/delta-radicalgrep/include/unicode/data/kRSKangXi.h).

`radicalcount.cpp` is the main framework for the Radical Count program. The auxilary functions and radical-set maps can be found in `radical_interface.h`.

## **radicalcount.cpp**

This file is the main framework of the Radical Count program. The LLVM input parser takes in two arguments; the radical expression and the filepaths(s). The radical expression is passed on to be parsed by `parse_input()`, and is then sent to `radicalcount1()` for processing.  `ucount1()` then generates the count result using the pipeline that was made by `radicalcount1()` earlier.

If more than one file is provided in input, Radical Count counts the occurences in each one and produces the total amount. Otherwise, the program returns the count as is.

* `radicalcount1()`:
 This function finds the requested unicode set for the inputted radical and makes a character class for it. It creates a property node of that unicode set and generates a pipeline. This pipeline is returned and used in `ucount1()`.

## **radical_interface.cpp & radical_interface.h**

The `radical_interface` files defines the namespace `BS` and its members, which are used in the Radical Count program. It also contains the parsing function `parse_input()`.

Members of class `UnicodeSetTable`:

* `map<string, const UCD::UnicodeSet*> _unicodeset_radical_table`: 
This map lists all 214 Kangxi radical sets and their respective keys. The key of each set is the Kangxi radical index, and the value is the corresponding unicode set. This is not used in the current iteration, but will be implemented later on.

* `map<string, const UCD::UnicodeSet*> radical_table`:
Instead of using a numeric key, the actual Kangxi radical is used and mapped to their corresponding values. Note that one unicode set may belong to different radicals (e.g. 水 and 氵both map to set 85).

* `get_uset()`:
This function finds the requested unicode set for the inputted radical, from `radical_table`. In `radicalcount.cpp`, this is invoked with the object `ucd_radical`.

`parse_input()`:
This function parses the inputted radical expression (e.g. 氵_ or 氵_子_ ) and stores it in a variable of type `input_radical`, which is predefined to represent a pair data structure. In `radicalcount.cpp`, `ci` is used to hold the parsed expression.


