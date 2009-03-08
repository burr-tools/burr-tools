# copy images from other directory to here
for i in ../doc_src/*.png
do
  cp $i .
done

# create a table of content for easier navigation in the t2t file
../doc_src/txt2tags.py --toc-only ../doc_src/userGuide.t2t > userGuide.t2t.toc

# create a pfd by first converting t2t to tex file and then running pdflatex
# run pdflatex 4 times to be sure all references are correct
../doc_src/txt2tags.py -t tex --toc -n -o - ../doc_src/userGuide.t2t > userGuide.tex
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
../doc_src/txt2tags.py -t html -o - ../doc_src/userGuide.t2t > userGuide.html
htmldoc --format htmlsep --toclevels 2 --outdir html userGuide.html
