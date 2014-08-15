while (<>) {
  if (/^VS_VERSION_INFO/) {
    while (<>) {
      $_ = <>, last if /^END/;
    }
  }
  $icons{$1}++ if /^(\w+)\s+(CURSOR|ICON)\s+DISCARDABLE\s+\"[\w.]+\"\s*$/;
  s/^icon\b/xicon/;
  print;
  print <<EOI if /\#include\s+<patchlevel\.h>/;
#include "tkwin.HHH"
EOI
}

open H, ">cursors.h" or die "Cannot open cursors.h: $!";
print H <<EOP;

#include "rc/tkwin.HHH"

typedef struct {
  char* name;
  int   id;
} myCursor;

static myCursor cursors[] = {
EOP

for (sort keys %icons) {
  $name = $_;
  $name = 'xicon' if $name eq 'icon';
  print H "  {\"$_\", $name},\n"
}

print H <<EOP;
  {NULL, 0}
};

EOP
close H or die "Cannot close cursors.h: $!";
