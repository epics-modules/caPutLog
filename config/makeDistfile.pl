eval 'exec perl -S $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if $running_under_some_shell; # makeDistfile.pl

# Generate the <TOP>/Distfile

$usage = 'Usage: perl -s makeDistfile -top=<top> -ver=<ver> -domains=<domainlist> -<domain>=<dir> ...
 where
  <top>: name of the top dir
  <ver>: version tag
  <domainlist>: quoted list of <domain> names
  <domain>: domain name (such as IOC, OPI, ...)
  <dir>: target dir for specified domain
Example:
  perl -s makeDistfile -top=test -ver=1999-12-24 -domains="IOC OPI" -IOC=/opt/IOC -OPI=/opt/OPI
';

defined $domains and defined $top or die $usage;

@domain_list = split /\s+/, $domains;

open(DISTFILE,">Distfile") or die "Can't open Distfile for writing: $!\n";

for $domain (@domain_list) {
	if (defined($$domain)) {
		if ($ver) {
			$target_dir = "$$domain/Releases/$top/$ver";
		} else {
			$target_dir = "$$domain/$top";
		}
		print DISTFILE

"$domain:
  \${FILES_$domain} -> \${HOSTS_$domain}
  install -owhole,nochkowner,nochkgroup,nochkmode
  $target_dir/. ;
  cmdspecial \"cd $target_dir; ./rdistCmd.$domain\" ;
"

	}
	else {
		die "no target directory specified for domain \"$domain\"\n";
	}
}

close DISTFILE;
