#ifndef DERIVEDJOININGTYPE_H
#define DERIVEDJOININGTYPE_H
/*
 *  Copyright (c) 2017 International Characters, Inc.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters, Inc.
 *
 *  This file is generated by UCD_properties.py - manual edits may be lost.
 */

#include "PropertyAliases.h"
#include "PropertyObjects.h"
#include "PropertyValueAliases.h"
#include "unicode_set.h"

namespace UCD {
  namespace JT_ns {
    const unsigned independent_prop_values = 6;
    /** Code Point Ranges for U
    [0000, 00ac], [00ae, 02ff], [0370, 0482], [048a, 0590], [05be, 05be],
    [05c0, 05c0], [05c3, 05c3], [05c6, 05c6], [05c8, 060f], [061b, 061b],
    [061d, 061f], [0621, 0621], [0660, 066d], [0674, 0674], [06d4, 06d4],
    [06dd, 06de], [06e5, 06e6], [06e9, 06e9], [06f0, 06f9], [06fd, 06fe],
    [0700, 070e], [074b, 074c], [0780, 07a5], [07b1, 07c9], [07f4, 07f9],
    [07fb, 0815], [081a, 081a], [0824, 0824], [0828, 0828], [082e, 083f],
    [0856, 0858], [085c, 089f], [08ad, 08ad], [08b5, 08b5], [08be, 08d3],
    [08e2, 08e2], [0903, 0939], [093b, 093b], [093d, 0940], [0949, 094c],
    [094e, 0950], [0958, 0961], [0964, 0980], [0982, 09bb], [09bd, 09c0],
    [09c5, 09cc], [09ce, 09e1], [09e4, 0a00], [0a03, 0a3b], [0a3d, 0a40],
    [0a43, 0a46], [0a49, 0a4a], [0a4e, 0a50], [0a52, 0a6f], [0a72, 0a74],
    [0a76, 0a80], [0a83, 0abb], [0abd, 0ac0], [0ac6, 0ac6], [0ac9, 0acc],
    [0ace, 0ae1], [0ae4, 0b00], [0b02, 0b3b], [0b3d, 0b3e], [0b40, 0b40],
    [0b45, 0b4c], [0b4e, 0b55], [0b57, 0b61], [0b64, 0b81], [0b83, 0bbf],
    [0bc1, 0bcc], [0bce, 0bff], [0c01, 0c3d], [0c41, 0c45], [0c49, 0c49],
    [0c4e, 0c54], [0c57, 0c61], [0c64, 0c80], [0c82, 0cbb], [0cbd, 0cbe],
    [0cc0, 0cc5], [0cc7, 0ccb], [0cce, 0ce1], [0ce4, 0d00], [0d02, 0d40],
    [0d45, 0d4c], [0d4e, 0d61], [0d64, 0dc9], [0dcb, 0dd1], [0dd5, 0dd5],
    [0dd7, 0e30], [0e32, 0e33], [0e3b, 0e46], [0e4f, 0eb0], [0eb2, 0eb3],
    [0eba, 0eba], [0ebd, 0ec7], [0ece, 0f17], [0f1a, 0f34], [0f36, 0f36],
    [0f38, 0f38], [0f3a, 0f70], [0f7f, 0f7f], [0f85, 0f85], [0f88, 0f8c],
    [0f98, 0f98], [0fbd, 0fc5], [0fc7, 102c], [1031, 1031], [1038, 1038],
    [103b, 103c], [103f, 1057], [105a, 105d], [1061, 1070], [1075, 1081],
    [1083, 1084], [1087, 108c], [108e, 109c], [109e, 135c], [1360, 1711],
    [1715, 1731], [1735, 1751], [1754, 1771], [1774, 17b3], [17b6, 17b6],
    [17be, 17c5], [17c7, 17c8], [17d4, 17dc], [17de, 1806], [1808, 1809],
    [180e, 181f], [1878, 1884], [18ab, 191f], [1923, 1926], [1929, 1931],
    [1933, 1938], [193c, 1a16], [1a19, 1a1a], [1a1c, 1a55], [1a57, 1a57],
    [1a5f, 1a5f], [1a61, 1a61], [1a63, 1a64], [1a6d, 1a72], [1a7d, 1a7e],
    [1a80, 1aaf], [1abf, 1aff], [1b04, 1b33], [1b35, 1b35], [1b3b, 1b3b],
    [1b3d, 1b41], [1b43, 1b6a], [1b74, 1b7f], [1b82, 1ba1], [1ba6, 1ba7],
    [1baa, 1baa], [1bae, 1be5], [1be7, 1be7], [1bea, 1bec], [1bee, 1bee],
    [1bf2, 1c2b], [1c34, 1c35], [1c38, 1ccf], [1cd3, 1cd3], [1ce1, 1ce1],
    [1ce9, 1cec], [1cee, 1cf3], [1cf5, 1cf7], [1cfa, 1dbf], [1df6, 1dfa],
    [1e00, 200a], [200c, 200c], [2010, 2029], [202f, 205f], [2065, 2069],
    [2070, 20cf], [20f1, 2cee], [2cf2, 2d7e], [2d80, 2ddf], [2e00, 3029],
    [302e, 3098], [309b, a66e], [a673, a673], [a67e, a69d], [a6a0, a6ef],
    [a6f2, a801], [a803, a805], [a807, a80a], [a80c, a824], [a827, a83f],
    [a873, a8c3], [a8c6, a8df], [a8f2, a925], [a92e, a946], [a952, a97f],
    [a983, a9b2], [a9b4, a9b5], [a9ba, a9bb], [a9bd, a9e4], [a9e6, aa28],
    [aa2f, aa30], [aa33, aa34], [aa37, aa42], [aa44, aa4b], [aa4d, aa7b],
    [aa7d, aaaf], [aab1, aab1], [aab5, aab6], [aab9, aabd], [aac0, aac0],
    [aac2, aaeb], [aaee, aaf5], [aaf7, abe4], [abe6, abe7], [abe9, abec],
    [abee, fb1d], [fb1f, fdff], [fe10, fe1f], [fe30, fefe], [ff00, fff8],
    [fffc, 101fc], [101fe, 102df], [102e1, 10375], [1037b, 10a00],
    [10a04, 10a04], [10a07, 10a0b], [10a10, 10a37], [10a3b, 10a3e],
    [10a40, 10abf], [10ac6, 10ac6], [10ac8, 10ac8], [10acb, 10acc],
    [10ae2, 10ae3], [10ae7, 10aea], [10af0, 10b7f], [10b92, 10ba8],
    [10baf, 11000], [11002, 11037], [11047, 1107e], [11082, 110b2],
    [110b7, 110b8], [110bb, 110bc], [110be, 110ff], [11103, 11126],
    [1112c, 1112c], [11135, 11172], [11174, 1117f], [11182, 111b5],
    [111bf, 111c9], [111cd, 1122e], [11232, 11233], [11235, 11235],
    [11238, 1123d], [1123f, 112de], [112e0, 112e2], [112eb, 112ff],
    [11302, 1133b], [1133d, 1133f], [11341, 11365], [1136d, 1136f],
    [11375, 11437], [11440, 11441], [11445, 11445], [11447, 114b2],
    [114b9, 114b9], [114bb, 114be], [114c1, 114c1], [114c4, 115b1],
    [115b6, 115bb], [115be, 115be], [115c1, 115db], [115de, 11632],
    [1163b, 1163c], [1163e, 1163e], [11641, 116aa], [116ac, 116ac],
    [116ae, 116af], [116b6, 116b6], [116b8, 1171c], [11720, 11721],
    [11726, 11726], [1172c, 11c2f], [11c37, 11c37], [11c3e, 11c3e],
    [11c40, 11c91], [11ca8, 11ca9], [11cb1, 11cb1], [11cb4, 11cb4],
    [11cb7, 16aef], [16af5, 16b2f], [16b37, 16f8e], [16f93, 1bc9c],
    [1bc9f, 1bc9f], [1bca4, 1d166], [1d16a, 1d172], [1d183, 1d184],
    [1d18c, 1d1a9], [1d1ae, 1d241], [1d245, 1d9ff], [1da37, 1da3a],
    [1da6d, 1da74], [1da76, 1da83], [1da85, 1da9a], [1daa0, 1daa0],
    [1dab0, 1dfff], [1e007, 1e007], [1e019, 1e01a], [1e022, 1e022],
    [1e025, 1e025], [1e02b, 1e8cf], [1e8d7, 1e8ff], [1e94b, e0000],
    [e0002, e001f], [e0080, e00ff], [e01f0, 10ffff]**/
    const UnicodeSet u_Set 
        {{{Full, 5}, {Mixed, 1}, {Full, 18}, {Empty, 3}, {Mixed, 1},
          {Full, 8}, {Mixed, 1}, {Full, 7}, {Mixed, 3}, {Full, 1},
          {Mixed, 2}, {Empty, 1}, {Mixed, 1}, {Empty, 2}, {Mixed, 3},
          {Empty, 1}, {Mixed, 1}, {Empty, 1}, {Full, 1}, {Mixed, 6},
          {Full, 2}, {Mixed, 24}, {Full, 1}, {Mixed, 1}, {Full, 1},
          {Mixed, 9}, {Full, 1}, {Mixed, 2}, {Full, 2}, {Mixed, 1},
          {Full, 2}, {Mixed, 2}, {Full, 2}, {Mixed, 2}, {Full, 1},
          {Mixed, 2}, {Full, 1}, {Mixed, 4}, {Full, 2}, {Mixed, 4},
          {Full, 21}, {Mixed, 1}, {Full, 29}, {Mixed, 4}, {Full, 1},
          {Mixed, 2}, {Full, 1}, {Mixed, 1}, {Empty, 2}, {Mixed, 3},
          {Full, 3}, {Mixed, 1}, {Full, 6}, {Mixed, 1}, {Full, 1},
          {Mixed, 2}, {Full, 1}, {Mixed, 1}, {Full, 2}, {Mixed, 6},
          {Full, 1}, {Mixed, 1}, {Full, 1}, {Mixed, 1}, {Full, 4},
          {Mixed, 2}, {Full, 6}, {Empty, 1}, {Mixed, 1}, {Full, 16},
          {Mixed, 2}, {Full, 1}, {Mixed, 1}, {Full, 2}, {Mixed, 2},
          {Full, 95}, {Mixed, 1}, {Full, 3}, {Mixed, 1}, {Full, 3},
          {Empty, 1}, {Full, 17}, {Mixed, 1}, {Full, 2}, {Mixed, 1},
          {Full, 942}, {Mixed, 2}, {Full, 2}, {Mixed, 1}, {Full, 8},
          {Mixed, 2}, {Empty, 1}, {Mixed, 1}, {Full, 2}, {Mixed, 2},
          {Full, 1}, {Mixed, 2}, {Full, 1}, {Mixed, 2}, {Full, 1},
          {Mixed, 1}, {Full, 1}, {Mixed, 3}, {Full, 1}, {Mixed, 3},
          {Full, 7}, {Mixed, 1}, {Full, 632}, {Mixed, 1}, {Full, 23},
          {Mixed, 2}, {Full, 5}, {Mixed, 1}, {Full, 7}, {Mixed, 1},
          {Full, 15}, {Mixed, 1}, {Full, 7}, {Mixed, 1}, {Full, 3},
          {Mixed, 1}, {Full, 52}, {Mixed, 2}, {Full, 4}, {Mixed, 2},
          {Full, 4}, {Mixed, 2}, {Full, 34}, {Mixed, 6}, {Full, 2},
          {Mixed, 2}, {Full, 1}, {Mixed, 4}, {Full, 2}, {Mixed, 1},
          {Full, 4}, {Mixed, 6}, {Full, 5}, {Mixed, 2}, {Full, 2},
          {Mixed, 2}, {Full, 6}, {Mixed, 2}, {Full, 2}, {Mixed, 2},
          {Full, 2}, {Mixed, 1}, {Full, 2}, {Mixed, 2}, {Full, 39},
          {Mixed, 1}, {Full, 2}, {Mixed, 2}, {Full, 625}, {Mixed, 1},
          {Full, 1}, {Mixed, 1}, {Full, 34}, {Mixed, 1}, {Full, 615},
          {Mixed, 2}, {Full, 165}, {Mixed, 3}, {Full, 4}, {Mixed, 1},
          {Full, 61}, {Empty, 1}, {Mixed, 1}, {Empty, 1}, {Mixed, 3},
          {Full, 42}, {Mixed, 2}, {Full, 68}, {Mixed, 1}, {Full, 1},
          {Empty, 2}, {Mixed, 1}, {Full, 24757}, {Mixed, 1}, {Empty, 3},
          {Full, 4}, {Empty, 7}, {Mixed, 1}, {Full, 6128}},
         {0xffffdfff, 0xffff0000, 0xfffffc07, 0x0001ffff, 0x40000000,
          0xffffff49, 0xe800ffff, 0x00000002, 0x00103fff, 0x60100000,
          0x63ff0260, 0x00007fff, 0x00001800, 0xfffe003f, 0x000003ff,
          0xfbf00000, 0x043fffff, 0xffffc110, 0xf1c00000, 0xc0202000,
          0x000fffff, 0x00000004, 0xfffffff8, 0xebffffff, 0xff01de01,
          0xfffffff3, 0xfffffffd, 0xefffffff, 0xffffdfe1, 0xfffffff3,
          0xfffffff9, 0xefffffff, 0xfffdc679, 0xffdcffff, 0xfffffff9,
          0xefffffff, 0xffffde41, 0xfffffff3, 0xfffffffd, 0x6fffffff,
          0xffbfdfe1, 0xfffffff3, 0xfffffffb, 0xffffdffe, 0xfffffffe,
          0x3fffffff, 0xff9fc23e, 0xfffffff3, 0xfffffffd, 0x6fffffff,
          0xffffcfbf, 0xfffffff3, 0xfffffffd, 0xffffdfe1, 0xfffffff3,
          0xffa3fbff, 0xf80dffff, 0xffff807f, 0xe40dffff, 0xffffc0ff,
          0xfcffffff, 0xfd5fffff, 0x8001ffff, 0x01001f20, 0xe0000000,
          0xffffffbf, 0x99021fff, 0x3cffffff, 0xffe1fffe, 0xdfffdf9b,
          0x1fffffff, 0xffe3ffff, 0xffe3ffff, 0xfff3ffff, 0xfff3ffff,
          0xc04fffff, 0xdff001bf, 0xffffc37f, 0xff000000, 0x0000001f,
          0xfffff800, 0xf1fbfe78, 0xf67fffff, 0x80bfffff, 0x6007e01a,
          0x8000ffff, 0xfffffff0, 0xe82fffff, 0xfffffffb, 0xfff007ff,
          0xfffffffc, 0xffffc4c3, 0xfffc5cbf, 0xff300fff, 0x0008ffff,
          0xfcefde02, 0x07c00000, 0xffff17ff, 0xffff83ff, 0xffff03e0,
          0x0000ffff, 0xfffe0000, 0xfffc7fff, 0x7fffffff, 0xffffc3ff,
          0xf9ffffff, 0xc0087fff, 0x3fffffff, 0xfffcffff, 0xfffff7bb,
          0xffffff9f, 0xfff80000, 0xffffffcf, 0xfffc0000, 0xffffc03f,
          0xfffc007f, 0xfffffff8, 0xec37ffff, 0xffffffdf, 0xff9981ff,
          0xffffeff7, 0xefffffff, 0x3e62ffff, 0xfffffffd, 0xffbfcfff,
          0xffffdedf, 0xbfffffff, 0xffff0000, 0xffff0000, 0x7fffffff,
          0xf1ffffff, 0xdfffffff, 0xfffffffe, 0xf83fffff, 0xffff0f91,
          0x78ffffff, 0x00001940, 0xffff078c, 0xfffc0000, 0xffff81ff,
          0xfffffffd, 0x00ffffff, 0xffffff80, 0x7fffffff, 0xfffffffc,
          0xd987ffff, 0xfffffff8, 0xffe0107f, 0xfff7ffff, 0xfffffffc,
          0x803fffff, 0xffffe3ff, 0xbf2c7fff, 0x7fffffff, 0xfffff807,
          0xfffffffc, 0xefffffff, 0xfffffffe, 0xffe0e03f, 0x00ffffff,
          0xffffffa3, 0x7a07ffff, 0xfffffff2, 0x4fc3ffff, 0xcffffffe,
          0x5807ffff, 0xfffffffe, 0xff40d7ff, 0x1fffffff, 0xfffff043,
          0x4080ffff, 0x0003ffff, 0xff920300, 0xffe0ffff, 0xff80ffff,
          0xfff87fff, 0x9fffffff, 0xfffffff0, 0x0007fc7f, 0xfffff018,
          0xffffc3ff, 0xffffffe3, 0x07800000, 0xffdfe000, 0x07ffffef,
          0xffff0001, 0x06000080, 0xfffff824, 0xff80ffff, 0xfffff800,
          0xfffffffd, 0xffff0000}};
    /** Code Point Ranges for C
    [0640, 0640], [07fa, 07fa], [180a, 180a], [200d, 200d]**/
    const UnicodeSet c_Set 
        {{{Empty, 50}, {Mixed, 1}, {Empty, 12}, {Mixed, 1}, {Empty, 128},
          {Mixed, 1}, {Empty, 63}, {Mixed, 1}, {Empty, 34559}},
         {0x00000001, 0x04000000, 0x00000400, 0x00002000}};
    /** Code Point Ranges for D
    [0620, 0620], [0626, 0626], [0628, 0628], [062a, 062e], [0633, 063f],
    [0641, 0647], [0649, 064a], [066e, 066f], [0678, 0687], [069a, 06bf],
    [06c1, 06c2], [06cc, 06cc], [06ce, 06ce], [06d0, 06d1], [06fa, 06fc],
    [06ff, 06ff], [0712, 0714], [071a, 071d], [071f, 0727], [0729, 0729],
    [072b, 072b], [072d, 072e], [074e, 0758], [075c, 076a], [076d, 0770],
    [0772, 0772], [0775, 0777], [077a, 077f], [07ca, 07ea], [0841, 0845],
    [0848, 0848], [084a, 0853], [0855, 0855], [08a0, 08a9], [08af, 08b0],
    [08b3, 08b4], [08b6, 08b8], [08ba, 08bd], [1807, 1807], [1820, 1877],
    [1887, 18a8], [18aa, 18aa], [a840, a871], [10ac0, 10ac4],
    [10ad3, 10ad6], [10ad8, 10adc], [10ade, 10ae0], [10aeb, 10aee],
    [10b80, 10b80], [10b82, 10b82], [10b86, 10b88], [10b8a, 10b8b],
    [10b8d, 10b8d], [10b90, 10b90], [10bad, 10bae], [1e900, 1e943]**/
    const UnicodeSet d_Set 
        {{{Empty, 49}, {Mixed, 4}, {Full, 1}, {Mixed, 6}, {Empty, 2},
          {Mixed, 2}, {Empty, 2}, {Mixed, 1}, {Empty, 2}, {Mixed, 1},
          {Empty, 122}, {Mixed, 1}, {Full, 2}, {Mixed, 3}, {Empty, 1148},
          {Full, 1}, {Mixed, 1}, {Empty, 786}, {Mixed, 2}, {Empty, 4},
          {Mixed, 2}, {Empty, 1770}, {Full, 2}, {Mixed, 1}, {Empty, 30901}},
         {0xfff87d41, 0x000006fe, 0xff00c000, 0xfc0000ff, 0x00035006,
          0x9c000000, 0xbc1c0000, 0x00006aff, 0xf1ffc000, 0xfce5e7ff,
          0xfffffc00, 0x000007ff, 0x002ffd3e, 0x3dd983ff, 0x00000080,
          0x00ffffff, 0xffffff80, 0x000005ff, 0x0003ffff, 0xdf78001f,
          0x00007801, 0x00012dc5, 0x00006000, 0x0000000f}};
    /** Code Point Ranges for R
    [0622, 0625], [0627, 0627], [0629, 0629], [062f, 0632], [0648, 0648],
    [0671, 0673], [0675, 0677], [0688, 0699], [06c0, 06c0], [06c3, 06cb],
    [06cd, 06cd], [06cf, 06cf], [06d2, 06d3], [06d5, 06d5], [06ee, 06ef],
    [0710, 0710], [0715, 0719], [071e, 071e], [0728, 0728], [072a, 072a],
    [072c, 072c], [072f, 072f], [074d, 074d], [0759, 075b], [076b, 076c],
    [0771, 0771], [0773, 0774], [0778, 0779], [0840, 0840], [0846, 0847],
    [0849, 0849], [0854, 0854], [08aa, 08ac], [08ae, 08ae], [08b1, 08b2],
    [08b9, 08b9], [10ac5, 10ac5], [10ac7, 10ac7], [10ac9, 10aca],
    [10ace, 10ad2], [10add, 10add], [10ae1, 10ae1], [10ae4, 10ae4],
    [10aef, 10aef], [10b81, 10b81], [10b83, 10b85], [10b89, 10b89],
    [10b8c, 10b8c], [10b8e, 10b8f], [10b91, 10b91], [10ba9, 10bac]**/
    const UnicodeSet r_Set 
        {{{Empty, 49}, {Mixed, 4}, {Empty, 1}, {Mixed, 6}, {Empty, 6},
          {Mixed, 1}, {Empty, 2}, {Mixed, 1}, {Empty, 2064}, {Mixed, 2},
          {Empty, 4}, {Mixed, 2}, {Empty, 32674}},
         {0x000782bc, 0x00000100, 0x00ee0000, 0x03ffff00, 0x002caff9,
          0x0000c000, 0x43e10000, 0x00009500, 0x0e002000, 0x031a1800,
          0x001002c1, 0x02065c00, 0x2007c6a0, 0x00008012, 0x0002d23a,
          0x00001e00}};
    /** Code Point Ranges for L
    [a872, a872], [10acd, 10acd], [10ad7, 10ad7]**/
    const UnicodeSet l_Set 
        {{{Empty, 1347}, {Mixed, 1}, {Empty, 786}, {Mixed, 1},
          {Empty, 32681}},
         {0x00040000, 0x00802000}};
    /** Code Point Ranges for T
    [00ad, 00ad], [0300, 036f], [0483, 0489], [0591, 05bd], [05bf, 05bf],
    [05c1, 05c2], [05c4, 05c5], [05c7, 05c7], [0610, 061a], [061c, 061c],
    [064b, 065f], [0670, 0670], [06d6, 06dc], [06df, 06e4], [06e7, 06e8],
    [06ea, 06ed], [070f, 070f], [0711, 0711], [0730, 074a], [07a6, 07b0],
    [07eb, 07f3], [0816, 0819], [081b, 0823], [0825, 0827], [0829, 082d],
    [0859, 085b], [08d4, 08e1], [08e3, 0902], [093a, 093a], [093c, 093c],
    [0941, 0948], [094d, 094d], [0951, 0957], [0962, 0963], [0981, 0981],
    [09bc, 09bc], [09c1, 09c4], [09cd, 09cd], [09e2, 09e3], [0a01, 0a02],
    [0a3c, 0a3c], [0a41, 0a42], [0a47, 0a48], [0a4b, 0a4d], [0a51, 0a51],
    [0a70, 0a71], [0a75, 0a75], [0a81, 0a82], [0abc, 0abc], [0ac1, 0ac5],
    [0ac7, 0ac8], [0acd, 0acd], [0ae2, 0ae3], [0b01, 0b01], [0b3c, 0b3c],
    [0b3f, 0b3f], [0b41, 0b44], [0b4d, 0b4d], [0b56, 0b56], [0b62, 0b63],
    [0b82, 0b82], [0bc0, 0bc0], [0bcd, 0bcd], [0c00, 0c00], [0c3e, 0c40],
    [0c46, 0c48], [0c4a, 0c4d], [0c55, 0c56], [0c62, 0c63], [0c81, 0c81],
    [0cbc, 0cbc], [0cbf, 0cbf], [0cc6, 0cc6], [0ccc, 0ccd], [0ce2, 0ce3],
    [0d01, 0d01], [0d41, 0d44], [0d4d, 0d4d], [0d62, 0d63], [0dca, 0dca],
    [0dd2, 0dd4], [0dd6, 0dd6], [0e31, 0e31], [0e34, 0e3a], [0e47, 0e4e],
    [0eb1, 0eb1], [0eb4, 0eb9], [0ebb, 0ebc], [0ec8, 0ecd], [0f18, 0f19],
    [0f35, 0f35], [0f37, 0f37], [0f39, 0f39], [0f71, 0f7e], [0f80, 0f84],
    [0f86, 0f87], [0f8d, 0f97], [0f99, 0fbc], [0fc6, 0fc6], [102d, 1030],
    [1032, 1037], [1039, 103a], [103d, 103e], [1058, 1059], [105e, 1060],
    [1071, 1074], [1082, 1082], [1085, 1086], [108d, 108d], [109d, 109d],
    [135d, 135f], [1712, 1714], [1732, 1734], [1752, 1753], [1772, 1773],
    [17b4, 17b5], [17b7, 17bd], [17c6, 17c6], [17c9, 17d3], [17dd, 17dd],
    [180b, 180d], [1885, 1886], [18a9, 18a9], [1920, 1922], [1927, 1928],
    [1932, 1932], [1939, 193b], [1a17, 1a18], [1a1b, 1a1b], [1a56, 1a56],
    [1a58, 1a5e], [1a60, 1a60], [1a62, 1a62], [1a65, 1a6c], [1a73, 1a7c],
    [1a7f, 1a7f], [1ab0, 1abe], [1b00, 1b03], [1b34, 1b34], [1b36, 1b3a],
    [1b3c, 1b3c], [1b42, 1b42], [1b6b, 1b73], [1b80, 1b81], [1ba2, 1ba5],
    [1ba8, 1ba9], [1bab, 1bad], [1be6, 1be6], [1be8, 1be9], [1bed, 1bed],
    [1bef, 1bf1], [1c2c, 1c33], [1c36, 1c37], [1cd0, 1cd2], [1cd4, 1ce0],
    [1ce2, 1ce8], [1ced, 1ced], [1cf4, 1cf4], [1cf8, 1cf9], [1dc0, 1df5],
    [1dfb, 1dff], [200b, 200b], [200e, 200f], [202a, 202e], [2060, 2064],
    [206a, 206f], [20d0, 20f0], [2cef, 2cf1], [2d7f, 2d7f], [2de0, 2dff],
    [302a, 302d], [3099, 309a], [a66f, a672], [a674, a67d], [a69e, a69f],
    [a6f0, a6f1], [a802, a802], [a806, a806], [a80b, a80b], [a825, a826],
    [a8c4, a8c5], [a8e0, a8f1], [a926, a92d], [a947, a951], [a980, a982],
    [a9b3, a9b3], [a9b6, a9b9], [a9bc, a9bc], [a9e5, a9e5], [aa29, aa2e],
    [aa31, aa32], [aa35, aa36], [aa43, aa43], [aa4c, aa4c], [aa7c, aa7c],
    [aab0, aab0], [aab2, aab4], [aab7, aab8], [aabe, aabf], [aac1, aac1],
    [aaec, aaed], [aaf6, aaf6], [abe5, abe5], [abe8, abe8], [abed, abed],
    [fb1e, fb1e], [fe00, fe0f], [fe20, fe2f], [feff, feff], [fff9, fffb],
    [101fd, 101fd], [102e0, 102e0], [10376, 1037a], [10a01, 10a03],
    [10a05, 10a06], [10a0c, 10a0f], [10a38, 10a3a], [10a3f, 10a3f],
    [10ae5, 10ae6], [11001, 11001], [11038, 11046], [1107f, 11081],
    [110b3, 110b6], [110b9, 110ba], [110bd, 110bd], [11100, 11102],
    [11127, 1112b], [1112d, 11134], [11173, 11173], [11180, 11181],
    [111b6, 111be], [111ca, 111cc], [1122f, 11231], [11234, 11234],
    [11236, 11237], [1123e, 1123e], [112df, 112df], [112e3, 112ea],
    [11300, 11301], [1133c, 1133c], [11340, 11340], [11366, 1136c],
    [11370, 11374], [11438, 1143f], [11442, 11444], [11446, 11446],
    [114b3, 114b8], [114ba, 114ba], [114bf, 114c0], [114c2, 114c3],
    [115b2, 115b5], [115bc, 115bd], [115bf, 115c0], [115dc, 115dd],
    [11633, 1163a], [1163d, 1163d], [1163f, 11640], [116ab, 116ab],
    [116ad, 116ad], [116b0, 116b5], [116b7, 116b7], [1171d, 1171f],
    [11722, 11725], [11727, 1172b], [11c30, 11c36], [11c38, 11c3d],
    [11c3f, 11c3f], [11c92, 11ca7], [11caa, 11cb0], [11cb2, 11cb3],
    [11cb5, 11cb6], [16af0, 16af4], [16b30, 16b36], [16f8f, 16f92],
    [1bc9d, 1bc9e], [1bca0, 1bca3], [1d167, 1d169], [1d173, 1d182],
    [1d185, 1d18b], [1d1aa, 1d1ad], [1d242, 1d244], [1da00, 1da36],
    [1da3b, 1da6c], [1da75, 1da75], [1da84, 1da84], [1da9b, 1da9f],
    [1daa1, 1daaf], [1e000, 1e006], [1e008, 1e018], [1e01b, 1e021],
    [1e023, 1e024], [1e026, 1e02a], [1e8d0, 1e8d6], [1e944, 1e94a],
    [e0001, e0001], [e0020, e007f], [e0100, e01ef]**/
    const UnicodeSet t_Set 
        {{{Empty, 5}, {Mixed, 1}, {Empty, 18}, {Full, 3}, {Mixed, 1},
          {Empty, 8}, {Mixed, 1}, {Empty, 7}, {Mixed, 3}, {Empty, 1},
          {Mixed, 1}, {Empty, 1}, {Mixed, 2}, {Empty, 2}, {Mixed, 5},
          {Empty, 2}, {Mixed, 1}, {Empty, 1}, {Mixed, 4}, {Empty, 3},
          {Mixed, 23}, {Empty, 1}, {Mixed, 1}, {Empty, 1}, {Mixed, 9},
          {Empty, 1}, {Mixed, 2}, {Empty, 2}, {Mixed, 1}, {Empty, 2},
          {Mixed, 2}, {Empty, 2}, {Mixed, 2}, {Empty, 1}, {Mixed, 2},
          {Empty, 1}, {Mixed, 4}, {Empty, 2}, {Mixed, 4}, {Empty, 21},
          {Mixed, 1}, {Empty, 29}, {Mixed, 4}, {Empty, 1}, {Mixed, 2},
          {Empty, 1}, {Mixed, 1}, {Empty, 3}, {Mixed, 2}, {Empty, 3},
          {Mixed, 1}, {Empty, 6}, {Mixed, 1}, {Empty, 1}, {Mixed, 2},
          {Empty, 1}, {Mixed, 1}, {Empty, 2}, {Mixed, 6}, {Empty, 1},
          {Mixed, 1}, {Empty, 1}, {Mixed, 1}, {Empty, 4}, {Mixed, 2},
          {Empty, 6}, {Full, 1}, {Mixed, 1}, {Empty, 16}, {Mixed, 2},
          {Empty, 1}, {Mixed, 1}, {Empty, 2}, {Mixed, 2}, {Empty, 95},
          {Mixed, 1}, {Empty, 3}, {Mixed, 1}, {Empty, 3}, {Full, 1},
          {Empty, 17}, {Mixed, 1}, {Empty, 2}, {Mixed, 1}, {Empty, 942},
          {Mixed, 2}, {Empty, 2}, {Mixed, 1}, {Empty, 8}, {Mixed, 2},
          {Empty, 4}, {Mixed, 2}, {Empty, 1}, {Mixed, 2}, {Empty, 1},
          {Mixed, 2}, {Empty, 1}, {Mixed, 1}, {Empty, 1}, {Mixed, 3},
          {Empty, 1}, {Mixed, 3}, {Empty, 7}, {Mixed, 1}, {Empty, 632},
          {Mixed, 1}, {Empty, 23}, {Mixed, 2}, {Empty, 5}, {Mixed, 1},
          {Empty, 7}, {Mixed, 1}, {Empty, 15}, {Mixed, 1}, {Empty, 7},
          {Mixed, 1}, {Empty, 3}, {Mixed, 1}, {Empty, 52}, {Mixed, 2},
          {Empty, 5}, {Mixed, 1}, {Empty, 40}, {Mixed, 6}, {Empty, 2},
          {Mixed, 2}, {Empty, 1}, {Mixed, 4}, {Empty, 2}, {Mixed, 1},
          {Empty, 4}, {Mixed, 6}, {Empty, 5}, {Mixed, 2}, {Empty, 2},
          {Mixed, 2}, {Empty, 6}, {Mixed, 2}, {Empty, 2}, {Mixed, 2},
          {Empty, 2}, {Mixed, 1}, {Empty, 2}, {Mixed, 2}, {Empty, 39},
          {Mixed, 1}, {Empty, 2}, {Mixed, 2}, {Empty, 625}, {Mixed, 1},
          {Empty, 1}, {Mixed, 1}, {Empty, 34}, {Mixed, 1}, {Empty, 615},
          {Mixed, 2}, {Empty, 165}, {Mixed, 3}, {Empty, 4}, {Mixed, 1},
          {Empty, 61}, {Full, 1}, {Mixed, 1}, {Full, 1}, {Mixed, 3},
          {Empty, 42}, {Mixed, 2}, {Empty, 68}, {Mixed, 1}, {Empty, 3},
          {Mixed, 1}, {Empty, 24757}, {Mixed, 1}, {Full, 3}, {Empty, 4},
          {Full, 7}, {Mixed, 1}, {Empty, 6128}},
         {0x00002000, 0x0000ffff, 0x000003f8, 0xfffe0000, 0xbfffffff,
          0x000000b6, 0x17ff0000, 0xfffff800, 0x00010000, 0x9fc00000,
          0x00003d9f, 0x00028000, 0xffff0000, 0x000007ff, 0x0001ffc0,
          0x000ff800, 0xfbc00000, 0x00003eef, 0x0e000000, 0xfff00000,
          0xfffffffb, 0x00000007, 0x14000000, 0x00fe21fe, 0x0000000c,
          0x00000002, 0x10000000, 0x0000201e, 0x0000000c, 0x00000006,
          0x10000000, 0x00023986, 0x00230000, 0x00000006, 0x10000000,
          0x000021be, 0x0000000c, 0x00000002, 0x90000000, 0x0040201e,
          0x0000000c, 0x00000004, 0x00002001, 0x00000001, 0xc0000000,
          0x00603dc1, 0x0000000c, 0x00000002, 0x90000000, 0x00003040,
          0x0000000c, 0x00000002, 0x0000201e, 0x0000000c, 0x005c0400,
          0x07f20000, 0x00007f80, 0x1bf20000, 0x00003f00, 0x03000000,
          0x02a00000, 0x7ffe0000, 0xfeffe0df, 0x1fffffff, 0x00000040,
          0x66fde000, 0xc3000000, 0x001e0001, 0x20002064, 0xe0000000,
          0x001c0000, 0x001c0000, 0x000c0000, 0x000c0000, 0x3fb00000,
          0x200ffe40, 0x00003800, 0x00000060, 0x00000200, 0x0e040187,
          0x09800000, 0x7f400000, 0x9ff81fe5, 0x7fff0000, 0x0000000f,
          0x17d00000, 0x00000004, 0x000ff800, 0x00000003, 0x00003b3c,
          0x0003a340, 0x00cff000, 0xfff70000, 0x031021fd, 0xf83fffff,
          0x0000c800, 0x00007c00, 0x0000fc1f, 0xffff0000, 0x0001ffff,
          0x00038000, 0x80000000, 0x00003c00, 0x06000000, 0x3ff78000,
          0xc0000000, 0x00030000, 0x00000844, 0x00000060, 0x00000030,
          0x0003ffff, 0x00003fc0, 0x0003ff80, 0x00000007, 0x13c80000,
          0x00000020, 0x00667e00, 0x00001008, 0x10000000, 0xc19d0000,
          0x00000002, 0x00403000, 0x00002120, 0x40000000, 0x0000ffff,
          0x0000ffff, 0x80000000, 0x0e000000, 0x20000000, 0x00000001,
          0x07c00000, 0x0000f06e, 0x87000000, 0x00000060, 0x00000002,
          0xff000000, 0x0000007f, 0x80000000, 0x00000003, 0x26780000,
          0x00000007, 0x001fef80, 0x00080000, 0x00000003, 0x7fc00000,
          0x00001c00, 0x40d38000, 0x80000000, 0x000007f8, 0x00000003,
          0x10000000, 0x00000001, 0x001f1fc0, 0xff000000, 0x0000005c,
          0x85f80000, 0x0000000d, 0xb03c0000, 0x30000001, 0xa7f80000,
          0x00000001, 0x00bf2800, 0xe0000000, 0x00000fbc, 0xbf7f0000,
          0xfffc0000, 0x006dfcff, 0x001f0000, 0x007f0000, 0x00078000,
          0x60000000, 0x0000000f, 0xfff80380, 0x00000fe7, 0x00003c00,
          0x0000001c, 0xf87fffff, 0x00201fff, 0xf8000010, 0x0000fffe,
          0xf9ffff7f, 0x000007db, 0x007f0000, 0x000007f0, 0x00000002,
          0x0000ffff}};
    static EnumeratedPropertyObject property_object
        {jt,
         JT_ns::independent_prop_values,
         JT_ns::enum_names,
         JT_ns::value_names,
         JT_ns::aliases_only_map,
         {&u_Set, &c_Set, &d_Set, &r_Set, &l_Set, &t_Set
         }};
    }
}

#endif
