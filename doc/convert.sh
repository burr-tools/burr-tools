# copy images from other directory to here
for i in ../doc_src/*.png
do
  cp $i .
done

# copy the user Guide

# first isolate the version from configure.ac
title=`head -n 1 ../configure.ac`
title=${title#AC_INIT(burrtools, }
title=${title%)}

# now prepend the automatically generated title
rm -f userGuide.t2t
echo User Guide for BurrTool $title > userGuide.t2t
cat ../doc_src/userGuide.t2t >> userGuide.t2t
chmod a-w userGuide.t2t   # just to make sure we don't start editing this file


# create a table of content for easier navigation in the t2t file
../doc_src/txt2tags.py --toc-only userGuide.t2t > userGuide.t2t.toc

# create a pfd by first converting t2t to tex file and then running pdflatex
# run pdflatex 4 times to be sure all references are correct
../doc_src/txt2tags.py -t tex --toc -n -o - userGuide.t2t > userGuide.tex
pdflatex userGuide.tex
pdflatex userGuide.tex
pdflatex userGuide.tex
pdflatex userGuide.tex
mv userGuide.pdf burrtools-$title-A4.pdf

sed 's/documentclass\[11pt,a4paper\]{book}/documentclass[11pt,letter]{book}/' -i userGuide.tex
pdflatex userGuide.tex
pdflatex userGuide.tex
pdflatex userGuide.tex
pdflatex userGuide.tex
mv userGuide.pdf burrtools-$title-letter.pdf

# create the html documentation
# first create the html output dir, or empty it, then convert the t2t to html
# then split the generated big html file into multiple htmls using htmldoc
if [ -d html ]
then
  echo emptying html dir
  cd html
  rm -f *
  cd ..
else
  echo creating html dir
  mkdir html
fi
../doc_src/txt2tags.py -t html -o - userGuide.t2t > userGuide.html
htmldoc --format htmlsep --toclevels 2 --outdir html userGuide.html

rm -f userGuide.t2t
