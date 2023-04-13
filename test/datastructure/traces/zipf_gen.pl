#!/usr/bin/perl -w

use Math::Random::Zipf;
use List::Util qw(shuffle);

$#ARGV==2 or die "usage: zipf_gen.pl range_max num_keys num_lookups\n";

($range_max, $num_keys, $num_lookups) = @ARGV;

$z = Math::Random::Zipf->new($range_max,1);

@arrayOfKeys = (0..$num_keys);

# random_set_seed(0, 0);
# random_seed_from_phrase("some string");
srand(0);
@arrayOfKeys = shuffle @arrayOfKeys;

# print $num_keys, " ", $num_lookups, "\n";

#generate key set with no collisions
for ($i=0;$i<$num_keys;$i++) {
    print "insert ",$arrayOfKeys[$i],"\n";  
}

#generate lookup stream, including lookups that
#are for non-existent keys
for ($i=0;$i<$num_lookups;$i++) {
    print "lookup ",$z->rand(),"\n";
}
    
