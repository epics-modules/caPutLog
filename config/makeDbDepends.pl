eval 'exec perl -S $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if $running_under_some_shell; # makeDbDepends.pl

# Called from within the object directory.
# Searches the .substitutions files (from the command line) for
# "file xxx {" entries to create a DEPENDS file

undef $/;

open(OUT, ">DEPENDS") or die "Cannot open DEPENDS: $!";

foreach $file (@ARGV) {
    open(IN, "<$file") or die "Cannot open $file: $!";
    $substfile = <IN>;
    close IN or die "Cannot close $file: $!";

    @depends = ($substfile =~ m/file\s+([^{\s]*)\s*\{/g);
    chomp @depends;

    if (@depends) {
	$file =~ s/\.substitutions/\.t.db.raw/;
	print OUT "${file}:: @depends\n";
    }
}
close OUT or die "Cannot close $file: $!";
