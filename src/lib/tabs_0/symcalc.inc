voxel_c * v;
bool res;
v = pp->getGridType()->getVoxel(pp);
v->transform(26);
res=pp->identicalInBB(v);
delete v;
if (res) {
v = pp->getGridType()->getVoxel(pp);
v->transform(2);
res=pp->identicalInBB(v);
delete v;
if (res) {
v = pp->getGridType()->getVoxel(pp);
v->transform(8);
res=pp->identicalInBB(v);
delete v;
if (res) {
v = pp->getGridType()->getVoxel(pp);
v->transform(1);
res=pp->identicalInBB(v);
delete v;
if (res) {
v = pp->getGridType()->getVoxel(pp);
v->transform(4);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)97; //FFFFFFFFFFFF
} else {
return (symmetries_t)57; //000F0F000F0F
}
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(4);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)68; //005555005555
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(5);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)96; //AAA5A5AAA5A5
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(16);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)92; //550505550505
} else {
return (symmetries_t)48; //000505000505
}
}
}
}
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(1);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)34; //00000F00000F
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(9);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)54; //000A05000A05
} else {
return (symmetries_t)32; //000005000005
}
}
}
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(6);
res=pp->identicalInBB(v);
delete v;
if (res) {
v = pp->getGridType()->getVoxel(pp);
v->transform(8);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)63; //001414004141
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(9);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)95; //812814248241
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(11);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)84; //128214482841
} else {
return (symmetries_t)36; //000014000041
}
}
}
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(10);
res=pp->identicalInBB(v);
delete v;
if (res) {
v = pp->getGridType()->getVoxel(pp);
v->transform(16);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)89; //440104110401
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(18);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)82; //110104440401
} else {
return (symmetries_t)39; //000104000401
}
}
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(14);
res=pp->identicalInBB(v);
delete v;
if (res) {
v = pp->getGridType()->getVoxel(pp);
v->transform(5);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)87; //211284844821
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(7);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)85; //181824424281
} else {
return (symmetries_t)59; //001004004001
}
}
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(8);
res=pp->identicalInBB(v);
delete v;
if (res) {
v = pp->getGridType()->getVoxel(pp);
v->transform(4);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)66; //004444001111
} else {
return (symmetries_t)45; //000404000101
}
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(5);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)86; //200084800021
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(7);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)75; //080024020081
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(9);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)52; //000804000201
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(11);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)42; //000204000801
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(13);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)74; //028004082001
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(15);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)94; //802004208001
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(18);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)70; //010004040001
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(22);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)77; //100004400001
} else {
return (symmetries_t)31; //000004000001
}
}
}
}
}
}
}
}
}
}
}
}
}
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(2);
res=pp->identicalInBB(v);
delete v;
if (res) {
v = pp->getGridType()->getVoxel(pp);
v->transform(8);
res=pp->identicalInBB(v);
delete v;
if (res) {
v = pp->getGridType()->getVoxel(pp);
v->transform(5);
res=pp->identicalInBB(v);
delete v;
if (res) {
v = pp->getGridType()->getVoxel(pp);
v->transform(1);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)29; //000000FFFFFF
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(25);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)93; //555A5AAAA5A5
} else {
return (symmetries_t)28; //000000AAA5A5
}
}
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(1);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)10; //000000000F0F
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(4);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)14; //000000005555
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(16);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)25; //000000550505
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(25);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)55; //000A0A000505
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(28);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)67; //005050000505
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(40);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)91; //550000000505
} else {
return (symmetries_t)7; //000000000505
}
}
}
}
}
}
}
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(32);
res=pp->identicalInBB(v);
delete v;
if (res) {
v = pp->getGridType()->getVoxel(pp);
v->transform(1);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)56; //000F0000000F
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(9);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)49; //00050A000A05
} else {
return (symmetries_t)47; //000500000005
}
}
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(1);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)2; //00000000000F
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(9);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)9; //000000000A05
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(25);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)33; //00000A000005
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(33);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)53; //000A00000005
} else {
return (symmetries_t)1; //000000000005
}
}
}
}
}
}
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(24);
res=pp->identicalInBB(v);
delete v;
if (res) {
v = pp->getGridType()->getVoxel(pp);
v->transform(8);
res=pp->identicalInBB(v);
delete v;
if (res) {
v = pp->getGridType()->getVoxel(pp);
v->transform(4);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)61; //001111001111
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(6);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)65; //004141004141
} else {
return (symmetries_t)38; //000101000101
}
}
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(10);
res=pp->identicalInBB(v);
delete v;
if (res) {
v = pp->getGridType()->getVoxel(pp);
v->transform(16);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)83; //110401110401
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(18);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)90; //440401440401
} else {
return (symmetries_t)44; //000401000401
}
}
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(9);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)41; //000201000201
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(11);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)51; //000801000801
} else {
return (symmetries_t)30; //000001000001
}
}
}
}
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(6);
res=pp->identicalInBB(v);
delete v;
if (res) {
v = pp->getGridType()->getVoxel(pp);
v->transform(8);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)13; //000000004141
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(9);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)20; //000000248241
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(11);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)24; //000000482841
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(34);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)62; //001400000041
} else {
return (symmetries_t)3; //000000000041
}
}
}
}
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(10);
res=pp->identicalInBB(v);
delete v;
if (res) {
v = pp->getGridType()->getVoxel(pp);
v->transform(16);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)18; //000000110401
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(18);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)23; //000000440401
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(40);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)81; //110000000401
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(42);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)88; //440000000401
} else {
return (symmetries_t)6; //000000000401
}
}
}
}
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(28);
res=pp->identicalInBB(v);
delete v;
if (res) {
v = pp->getGridType()->getVoxel(pp);
v->transform(8);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)60; //001010000101
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(13);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)79; //100210082001
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(14);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)46; //000410004001
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(15);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)72; //010810208001
} else {
return (symmetries_t)35; //000010000001
}
}
}
}
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(5);
res=pp->identicalInBB(v);
delete v;
if (res) {
v = pp->getGridType()->getVoxel(pp);
v->transform(11);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)27; //000000844821
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(33);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)73; //011200800021
} else {
return (symmetries_t)26; //000000800021
}
}
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(7);
res=pp->identicalInBB(v);
delete v;
if (res) {
v = pp->getGridType()->getVoxel(pp);
v->transform(9);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)22; //000000424281
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(35);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)80; //101800020081
} else {
return (symmetries_t)15; //000000020081
}
}
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(8);
res=pp->identicalInBB(v);
delete v;
if (res) {
v = pp->getGridType()->getVoxel(pp);
v->transform(4);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)11; //000000001111
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(30);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)64; //004040000101
} else {
return (symmetries_t)4; //000000000101
}
}
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(32);
res=pp->identicalInBB(v);
delete v;
if (res) {
v = pp->getGridType()->getVoxel(pp);
v->transform(18);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)78; //100100040001
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(22);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)71; //010100400001
} else {
return (symmetries_t)37; //000100000001
}
}
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(9);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)5; //000000000201
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(11);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)8; //000000000801
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(13);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)17; //000000082001
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(14);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)12; //000000004001
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(15);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)19; //000000208001
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(18);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)16; //000000040001
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(22);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)21; //000000400001
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(33);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)40; //000200000001
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(34);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)43; //000400000001
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(35);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)50; //000800000001
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(36);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)58; //001000000001
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(40);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)69; //010000000001
} else {
v = pp->getGridType()->getVoxel(pp);
v->transform(44);
res=pp->identicalInBB(v);
delete v;
if (res) {
return (symmetries_t)76; //100000000001
} else {
return (symmetries_t)0; //000000000001
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
