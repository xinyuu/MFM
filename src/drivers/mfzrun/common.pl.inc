###
# Begin: common.pl.inc

use Carp;
use File::Path qw(make_path);
use File::Spec;
use File::Temp qw(tempdir);
use IO::Uncompress::Unzip qw($UnzipError);
use Digest::SHA qw(sha512_hex);

sub VERSION { 1.0 }
sub MFZRUN_HEADER { "MFZ(".VERSION.")\n" }
sub MFZ_PUBKEY_NAME { "MFZPUBKEY.DAT" }
sub MFZ_SIG_NAME { "MFZSIG.DAT" }
sub MFZ_ZIP_NAME { "MFZ.ZIP" }

my $programName = $0;  # default
sub SetProgramName {
    $programName = shift;
}

sub UDie {
    my $msg = shift;
    print STDERR "\nError: $msg\n";
    print STDERR "Type '$programName help' for help\n";
    exit(1);
}

sub IDie {
    my $msg = shift;
    print STDERR "\nInternal Error: $msg\n";
    confess "I suck";
}

my $userHomeDir = glob "~/.mfm";
sub InitMFMSubDir {
    my $sub = shift;
    IDie "No sub?" unless defined $sub;
    my $dir = "$userHomeDir/$sub";
    $dir =~ /^(.+)$/ or IDie("WTF?");
    $dir = $1;  # Great.  Just great.

    if (!-d $dir) {
        make_path($dir)
            or die "Couldn't mkdir $dir: $!";
    }
    return $dir;
}

sub GetPublicKeyDir {
    return InitMFMSubDir("public_keys");
}

sub GetPublicKeyFile {
    my $handle = shift;
    my $ehandle = EscapeHandle($handle);
    my $dir = GetPublicKeyDir();
    my $pub = "$dir/$ehandle.pub";

    return $pub;
}

sub JoinHandleToKey {
    my ($handle, $keydata) = @_;
    my $data ="[MFM-Handle:$handle]\n$keydata";
    return $data;
}

sub SplitHandleFromKey {
    my $data = shift;
    $data =~ s/^\[MFM-Handle:(:?[a-zA-Z][-., a-zA-Z0-9]{0,62})\]\r?\n//
        or return undef;
    my $handle = $1;
    return ($handle,$data);
}

sub ComputeFingerprintFromFullPublicKey {
    my $fullpubkey = shift;
    my $fingerprint = lc(sha512_hex($fullpubkey));
    $fingerprint =~ s/^(....)(...)(....).+$/$1-$2-$3/
        or IDie("you're a cow");
    return $fingerprint;
}

sub ReadPublicKeyFile {
    my $handle = shift;
    my $file = GetPublicKeyFile($handle);
    my $data = ReadWholeFile($file);
    my ($pubhandle, $key) = SplitHandleFromKey($data);
    UDie("Bad format in public key file for '$handle'")
        unless defined $pubhandle;
    UDie("Non-matching handle in public key file ('$handle' vs '$pubhandle')")
        unless $pubhandle eq $handle;
    return ($key, ComputeFingerprintFromFullPublicKey($data));
}

sub GetPrivateKeyDir {
    my $privdir = InitMFMSubDir("private_keys");
    chmod 0700, $privdir;
    return $privdir;
}

sub GetPrivateKeyFile {
    my $handle = shift;
    my $ehandle = EscapeHandle($handle);
    my $dir = GetPrivateKeyDir();
    my $pub = "$dir/$ehandle.priv";
    return $pub;
}

sub ReadPrivateKeyFile {
    my $handle = shift;
    my $file = GetPrivateKeyFile($handle);
    my $data = ReadWholeFile($file);
    my ($privhandle, $key) = SplitHandleFromKey($data);
    UDie("Bad format in private key file for '$handle'")
        unless defined $privhandle;
    UDie("Non-matching handle in private key file ('$handle' vs '$privhandle')")
        unless $privhandle eq $handle;
    return $key;
}

sub VersionExit {
    print VERSION."\n";
    exit(0);
}

sub NextArg {
    my $arg = shift @ARGV;
    return $arg;
}

sub RestOfArgs {
    my @args = @ARGV;
    @ARGV = ();
    return @args;
}

sub ReadableFileOrDie {
    my ($text, $path) = @_;
    UDie "No $text provided" unless defined $path;
    UDie "Non-existent or unreadable $text: '$path'"
        unless -r $path and -f $path;
    $path =~ /^(.+)$/
      or IDie("am i here all alone");
    return $1;
}

sub WritableFileOrDie {
    my ($text, $path) = @_;
    UDie "No $text provided" unless defined $path;
    UDie "Unwritable $text: '$path': $!" unless -w $path or !-e $path;
    $path =~ /^(.+)$/
      or IDie("hands you a bone");
    return $1;
}

sub ReadWholeFile {
    my $file = shift;
    open (my $fh, "<", $file) or IDie("Can't read '$file': $!");
    local $/ = undef;
    my $content = <$fh>;
    close $fh or IDie("Failed closing '$file': $!");
    return $content;
}

sub WriteWholeFile {
    my ($file, $content, $perm) = @_;
    open (my $fh, ">", $file) or UDie("Can't write '$file': $!");
    chmod $perm, $fh
        if defined $perm;
    print $fh $content;
    close $fh or IDie("Failed closing '$file': $!");
}

sub UntaintHandleIfLegal {
    my $ref = shift;
    return 0
        unless $$ref =~ /^ *(:?[a-zA-Z][-., a-zA-Z0-9]{0,62}) *$/;
    $$ref = $1;
    return 1;
}

sub EscapeHandle {
    my $handle = shift;
    chomp($handle);
    $handle =~ s/([^a-zA-Z0-9])/sprintf("%%%02x",ord($1))/ge;
    return $handle;
}

sub UnzipStreamToMemory {
    my ($u) = @_;
    my @paths;

    my $status;
    my $count = 0;
    for ($status = 1; $status > 0; $status = $u->nextStream(), ++$count) {
        my $header = $u->getHeaderInfo();
        my $stored_time = $header->{'Time'};
        $stored_time =~ /^(\d+)$/ or die "Bad stored time: '$stored_time'";
        $stored_time = $1;  # Untainted

        my $fullpath = $header->{Name};
        my (undef, $path, $name) = File::Spec->splitpath($fullpath);

        if ($name eq "" or $name =~ m!/$!) {
            last if $status < 0;
        } else {

            my $data = "";
            my $buff;
            while (($status = $u->read($buff)) > 0) {
                $data .= $buff;
            }
            if ($status == 0) {
                push @paths, [$path, $name, $stored_time, $data];
            }
        }
    }

    die "Error in processing: $!\n"
        if $status < 0 ;
    return @paths;
}

sub UnzipStream {
    my ($u, $dest) = @_;
    my @paths;

    $dest = "." unless defined $dest;

    my $status;
    my $count = 0;
    for ($status = 1; $status > 0; $status = $u->nextStream(), ++$count) {
        my $header = $u->getHeaderInfo();
        my $stored_time = $header->{'Time'};
        $stored_time =~ /^(\d+)$/ or die "Bad stored time: '$stored_time'";
        $stored_time = $1;  # Untainted

        my (undef, $path, $name) = File::Spec->splitpath($header->{Name});
        my $destdir = "$dest/$path";

        my $totouch;
        unless (-d $destdir) {
            make_path($destdir)
                or die "Couldn't mkdir $destdir: $!";
            $totouch = $destdir;
        }
        if ($name eq "" or $name =~ m!/$!) {
            last if $status < 0;
        } else {

            my $destfile = "$destdir$name";
            my $buff;
#            print STDERR "Writing $destfile\n";
            my $fh = IO::File->new($destfile, "w")
                or die "Couldn't write to $destfile: $!";
            while (($status = $u->read($buff)) > 0) {
#                print STDERR "Read ".length($buff)."\n";
                $fh->write($buff);
            }
            $fh->close();
            $totouch = $destfile;
            push @paths, [$destdir, $name, $stored_time];
        }

        utime ($stored_time, $stored_time, $totouch)
            or die "Couldn't touch $totouch: $!";
    }

    die "Error in processing: $!\n"
        if $status < 0 ;
    return @paths;
}

# Returns:
# undef if $findName (in option $findPath) is not found,
# [$destdir, $name, $stored_time] if $pref is data from UnzipStream
# [$path, $name, $stored_time, $data] if $pref is data from UnzipStreamToMemory

sub FindName {
    my ($pref, $findName, $findPath) = @_;
    my @precs = @{$pref};
    for my $rec (@precs) {
        my @fields = @{$rec};
        my ($path, $name) = @fields;
        if ($name eq $findName) {
            if (!defined($findPath) || $path eq $findPath) {
                return @fields;
            }
        }
    }
    return undef;
}



# End: common.pl.inc
###
