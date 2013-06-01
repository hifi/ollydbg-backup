OllyDbg v1.10 backup plugin
===========================

This simple plugin allows saving the loaded executable labels and comments to a
CSV file and later loading them back in. You can store, share, modify and merge
your notes with ease and never lose your notes because OllyDbg flipped and threw 
them all out (it can happen).

The CSV file structure is as follow:

    RVA,label,comment

Output files are compatible with more feature rich *pyudd* which was the
inspiration for this plugin. Kudos to *libcsv* for easy to use CSV library for
the C programming language.

pyudd - http://code.google.com/p/pyudd/
libcsv - http://sourceforge.net/projects/libcsv/
