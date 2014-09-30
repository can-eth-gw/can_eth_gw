GNU Man Page
===========

	(!) Only edit the *.markdown files!

The script `pandoc-convert.sh` will convert to other formats including *man*, so do not touch the other files, since they will be overwritten. The script will parse all files which match `*.markdown`, so name all other markdown files `.md`. 

	(!) Make sure that you run the script bevore committing!

Format of `*.markdown` files
-------------------------

All *man pages* should be created here. Create a *pandoc markdown* file with the name `COMMAND.markdown` with a man page header.

The script assumes that the first line of the `*.markdown` files begin with the book name:

	% CAN-ETH-GW(SECTION-NUMBER) TITLE | VERSION
    % AUTHOR1
      AUTHOR2
    % DATE

 + The file must have UNIX Line Endings
 + Use `:` for a definition
 + Tables must be in Github Flavored Markdow format and indented to a code block