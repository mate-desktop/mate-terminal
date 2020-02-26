#!/usr/bin/perl
=pod

    update-authors.pl is part of MATE Terminal.

    MATE Terminal is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    MATE Terminal is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with MATE Terminal.  If not, see <http://www.gnu.org/licenses/>.

=cut
use strict;
use warnings;

sub ReplaceAuthors {
  my @authors = @_;
  $_ eq 'bl0ckeduser <bl0ckedusersoft%gmail.com>' and $_ = 'Gabriel Cormier-Affleck <bl0ckedusersoft%gmail.com>' for @authors;
  $_ eq 'hekel <hekel%archlinux.info>' and $_ = 'Adam Erdman <hekel%archlinux.info>' for @authors;
  $_ eq 'infirit <infirit%gmail.com>' and $_ = 'Sander Sweers <infirit%gmail.com>' for @authors;
  $_ eq 'leigh123linux <leigh123linux%googlemail.com>' and $_ = 'Leigh Scott <leigh123linux%googlemail.com>' for @authors;
  $_ eq 'lyokha <alexey.radkov%gmail.com>' and $_ = 'Alexey Radkov <alexey.radkov%gmail.com>' for @authors;
  $_ eq 'Martin Wimpress <code%flexion.org>' and $_ = 'Martin Wimpress <martin%mate-desktop.org>' for @authors;
  $_ eq 'monsta <monsta%inbox.ru>' and $_ = 'Vlad Orlov <monsta%inbox.ru>' for @authors;
  $_ eq 'Monsta <monsta%inbox.ru>' and $_ = 'Vlad Orlov <monsta%inbox.ru>' for @authors;
  $_ eq 'osch <oliver at luced de>' and $_ = 'osch <oliver%luced.de>' for @authors;
  $_ eq 'raveit65 <chat-to-me%raveit.de>' and $_ = 'Wolfgang Ulbrich <mate%raveit.de>' for @authors;
  $_ eq 'raveit65 <mate%raveit.de>' and $_ = 'Wolfgang Ulbrich <mate%raveit.de>' for @authors;
  $_ eq 'rbuj <robert.buj%gmail.com>' and $_ = 'Robert Buj <robert.buj%gmail.com>' for @authors;
  $_ eq 'Scott Balneaves <sbalneav%ltsp.org>' and $_ = 'Scott Balneaves <sbalneav%mate-desktop.org>' for @authors;
  $_ eq 'spuhpointer <madars.vitolins%gmail.com>' and $_ = 'Madars Vitolins <madars.vitolins%gmail.com>' for @authors;
  $_ eq 'Wolfgang Ulbrich <chat-to-me%raveit.de>' and $_ = 'Wolfgang Ulbrich <mate%raveit.de>' for @authors;
  return @authors;
}

sub GetCurrentAuthors {
  my @authors;
  open(FILE,"src/terminal.about") or die "Can't open src/terminal.about";
  while (<FILE>) {
    if (/^Authors=*(.+)$/) {
      @authors=split(";",$1);
    }
  }
  close FILE;
  return ReplaceAuthors(@authors);
}

sub GetNewAuthors {
  my @authors = `git log --pretty="%an <%ae>" --since "2012-01-01" -- . "_.h" "_.c" | sort | uniq | sed 's/@/%/g' | sed '/^mate-i18n.*/d'`;
  chomp @authors;
  return ReplaceAuthors(@authors);
}

my @A = GetCurrentAuthors;
my @B = GetNewAuthors;
for (@B) {
  s/<\d+\+(.+?)%users\.noreply\.github\.com>/<$1%users\.noreply\.github\.com>/;
  s/<(.+?)%users\.noreply\.github\.com>/https:\/\/github.com\/$1/;
}
my @merged = sort { $a cmp $b } keys %{{map {($_ => 1)} (@A, @B)}};
print join(';',@merged) . ';';
