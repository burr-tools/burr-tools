# create a table of content for easier navigation in the t2t file
./txt2tags.py --toc-only userGuide.t2t > userGuide.t2t.toc

# create a pfd by first converting t2t to tex file and then running pdflatex
# run pdflatex 4 times to be sure all references are correct
./txt2tags.py -t tex --toc -n userGuide.t2t
pdflatex userGuide.tex
pdflatex userGuide.tex
pdflatex userGuide.tex
pdflatex userGuide.tex

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
./txt2tags.py -t html userGuide.t2t
htmldoc --format htmlsep --toclevels 2 --outdir html userGuide.html
