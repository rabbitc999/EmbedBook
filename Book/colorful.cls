%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%% start of file `colorful.cls'.
%% 
%% Copyright 2023-2024 colorful (rabbitcandy521@outlook.com)
%% This work may be distributed and/or modified freely
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% % !Mode:: "TeX:UTF-8"
\NeedsTeXFormat{LaTeX2e}
\ProvidesClass{colorful}[2023/05/20 v6.0-alpha Colorful doc]

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\RequirePackage{kvoptions}
\RequirePackage{etoolbox}
\SetupKeyvalOptions{family=COLORFUL, prefix=COLORFUL@, setkeys=\kvsetkeys}
\newcommand{\ekv}[1]{\kvsetkeys{COLORFUL}{#1}}

%%%%%%%%%%%%%%%%%选项%%%%%%%%%%%%%%%%%
\DeclareStringOption[cn]{lang}
\DeclareStringOption[answer]{result}
\DeclareStringOption[fancy]{mode}
\DeclareStringOption[normal]{device}
\DeclareStringOption[cm]{math}
\DeclareStringOption[marginfalse]{marginpar}
\DeclareStringOption[onecol]{toc}
\DeclareStringOption{scheme}
\DeclareStringOption[ctexfont]{chinesefont}
\DeclareStringOption[numeric-comp]{citestyle}
\DeclareStringOption[numeric]{bibstyle}
\DeclareStringOption[chapter]{thmcnt}
\DeclareStringOption[biber]{bibend}
\DeclareStringOption[hang]{titlestyle}[hang]
\DeclareStringOption{cod}
\DeclareVoidOption{chinese}{\ekv{scheme=chinese}}
\DeclareVoidOption{ctexfont}{\ekv{chinesefont=ctexfont}}
\DeclareVoidOption{founder}{\ekv{chinesefont=founder}}
\DeclareVoidOption{nofont}{\ekv{chinesefont=nofont}}
\DeclareVoidOption{cn}{\ekv{lang=cn}}
\DeclareVoidOption{fancy}{\ekv{mode=fancy}}
\DeclareVoidOption{simple}{\ekv{mode=simple}}
\DeclareVoidOption{answer}{\ekv{result=answer}}
\DeclareVoidOption{noanswer}{\ekv{result=noanswer}}
\DeclareVoidOption{normal}{\ekv{device=normal}}
\DeclareVoidOption{pad}{\ekv{device=pad}}
\DeclareVoidOption{newtx}{\ekv{math=newtx}}
\DeclareVoidOption{mtpro2}{\ekv{math=mtpro2}}
\DeclareVoidOption{cm}{\ekv{math=cm}}
\DeclareVoidOption{margintrue}{\ekv{marginpar=margintrue}}
\DeclareVoidOption{marginfalse}{\ekv{marginpar=marginfalse}}
\DeclareVoidOption{onecol}{\ekv{toc=onecol}}
\DeclareVoidOption{twocol}{\ekv{toc=twocol}}
\DeclareVoidOption{chapter}{\ekv{thmcnt=chapter}}
\DeclareVoidOption{section}{\ekv{thmcnt=section}}
\DeclareVoidOption{minted}{\ekv{cod=minted}}
\DeclareVoidOption{biber}{\ekv{bibend=biber}}
\DeclareVoidOption{bibtex}{\ekv{bibend=bibtex}}
\DeclareVoidOption{hang}{\ekv{titlestyle=hang}}
\DeclareVoidOption{display}{\ekv{titlestyle=display}}
\DeclareDefaultOption{\PassOptionsToClass{\CurrentOption}{book}}
\newcommand\mailto[1]{\href{mailto:#1}{\nolinkurl{#1}}}

\ProcessKeyvalOptions*\relax
\LoadClass[a4paper,oneside]{book}

\RequirePackage{setspace}
\RequirePackage{csquotes}

\RequirePackage{hyperref}
\hypersetup{
  breaklinks,
  unicode,
  linktoc=all,
  bookmarksnumbered=true,
  bookmarksopen=true,
  pdfkeywords={COLORFULBook},
  colorlinks,
  linkcolor=pink,
  citecolor=winered,
  urlcolor=winered,
  plainpages=false,
  pdfstartview=FitH,
  pdfborder={0 0 0},
  linktocpage
}
\newif\ifCOLORFUL@Hy@later
  \@ifpackagelater{hyperref}{2022/05/16}
    {\COLORFUL@Hy@latertrue}
    {\COLORFUL@Hy@laterfalse}

%% device settings
\RequirePackage{geometry}
\ifdefstring{\COLORFUL@device}{normal}{
  \geometry{
    a4paper,
    top=25.4mm, bottom=25.4mm,
    left=20mm, right=20mm,
    headheight=2.17cm,
    headsep=4mm,
    footskip=12mm
  }
  \ifdefstring{\COLORFUL@marginpar}{margintrue}{
    \geometry{
      marginparwidth=5cm, marginparsep=5mm,
      left=2cm,right=7cm}}{\relax}}{
    \relax}

\ifdefstring{\COLORFUL@device}{pad}{
\geometry{
  paperwidth=7.5in, 
  paperheight=10in,
  margin=16mm,
  headheight=2.17cm,
  footskip=4mm
}}{\relax}


\RequirePackage{indentfirst,comment}
% fontsetting
\ifdefstring{\COLORFUL@math}{mtpro2}{
  \let\Bbbk\relax
  \RequirePackage[lite]{mtpro2}
}{\relax}

\setcounter{tocdepth}{1}
\renewcommand{\baselinestretch}{1.2}

\PassOptionsToPackage{no-math}{fontspec}
\PassOptionsToPackage{quiet}{fontspec}
\RequirePackage{iftex}

\ifpdftex
  \RequirePackage{newtxtext}
  \RequirePackage[scaled=.90]{helvet}
\else
  \RequirePackage[no-math]{fontspec}
  \setmainfont{TeXGyreTermesX}[
    UprightFont = *-Regular ,
    BoldFont = *-Bold ,
    ItalicFont = *-Italic ,
    BoldItalicFont = *-BoldItalic ,
    Extension = .otf ,
    Scale = 1.0]
    
  \setsansfont{texgyreheros}[
    UprightFont = *-regular ,
    BoldFont = *-bold ,
    ItalicFont = *-italic ,
    BoldItalicFont = *-bolditalic ,
    Extension = .otf ,
    Scale = 0.9]
\fi

\ifdefstring{\COLORFUL@lang}{cn}{
  \ifdefstring{\COLORFUL@chinesefont}{founder}{
    \RequirePackage[UTF8, scheme=plain, fontset=none]{ctex}
    \setCJKmainfont[BoldFont={FZHei-B01},ItalicFont={FZKai-Z03}]{FZShuSong-Z01}
    \setCJKsansfont[BoldFont={FZHei-B01}]{FZKai-Z03}
    \setCJKmonofont[BoldFont={FZHei-B01}]{FZFangSong-Z02}
    \setCJKfamilyfont{zhsong}{FZShuSong-Z01}
    \setCJKfamilyfont{zhhei}{FZHei-B01}
    \setCJKfamilyfont{zhkai}[BoldFont={FZHei-B01}]{FZKai-Z03}
    \setCJKfamilyfont{zhfs}[BoldFont={FZHei-B01}]{FZFangSong-Z02}
    \newcommand*{\songti}{\CJKfamily{zhsong}}
    \newcommand*{\heiti}{\CJKfamily{zhhei}}
    \newcommand*{\kaishu}{\CJKfamily{zhkai}}
    \newcommand*{\fangsong}{\CJKfamily{zhfs}}}{\relax}
  
  \ifdefstring{\COLORFUL@chinesefont}{nofont}{
    \RequirePackage[UTF8,scheme=plain,fontset=none]{ctex}
    \xeCJKsetup{AutoFakeBold=true}
    }{\relax}

  \ifdefstring{\COLORFUL@chinesefont}{ctexfont}{
    \RequirePackage[UTF8,scheme=plain]{ctex}
    \xeCJKsetup{AutoFakeBold=true}
    }{\relax}
  
  \AfterEndPreamble{
    \setlength\parindent{2\ccwd}}
}{\relax}

\ifcsname heiti\endcsname
  \newcommand{\cbfseries}{\heiti}
\else
  \newcommand{\cbfseries}{\bfseries}
\fi

\ifcsname kaishu\endcsname
  \newcommand{\citshape}{\kaishu}
\else
  \newcommand{\citshape}{\itshape}
\fi
\ifcsname kaishu\endcsname
  \newcommand{\cnormal}{\kaishu}
\else
  \newcommand{\cnormal}{\normalfont}
\fi

\ifcsname fangsong\endcsname
  \newcommand{\cfs}{\fangsong}
\else
  \newcommand{\cfs}{\normalfont}
\fi

\RequirePackage{anyfontsize}
\ifdefstring{\COLORFUL@math}{newtx}{
  \let\oldencodingdefault\encodingdefault
  \let\oldrmdefault\rmdefault
  \let\oldsfdefault\sfdefault
  \let\oldttdefault\ttdefault
  \def\encodingdefault{T1}
  \renewcommand{\rmdefault}{ntxtlf}
  \renewcommand{\sfdefault}{qhv}
  \renewcommand{\ttdefault}{ntxtt}
  \RequirePackage{newtxmath}
  \let\encodingdefault\oldencodingdefault
  \let\rmdefault\oldrmdefault
  \let\sfdefault\oldsfdefault
  \let\ttdefault\oldttdefault
  \let\Bbbk\relax
  \RequirePackage{esint}

  \DeclareSymbolFont{CMlargesymbols}{OMX}{cmex}{m}{n}
  \let\sumop\relax\let\prodop\relax
  \DeclareMathSymbol{\sumop}{\mathop}{CMlargesymbols}{"50}
  \DeclareMathSymbol{\prodop}{\mathop}{CMlargesymbols}{"51}
}{\relax}

% ----- Handle Colors -----
%% 章节以及页脚图形
\RequirePackage[table]{xcolor}
\definecolor{deepBlue}{RGB}{61, 124, 222}
\definecolor{lightergray}{gray}{0.99}
\definecolor{azures1}{RGB}{250, 252, 254}
\definecolor{azures2}{RGB}{41, 119, 196}
\definecolor{Beige}{RGB}{245, 245, 220}
\definecolor{Rhodamine}{RGB}{239, 84, 159}
\definecolor{Magenta}{RGB}{236, 0, 141}
\definecolor{RubineRed}{RGB}{236, 2, 126}
\definecolor{DarkCyan}{RGB}{0, 140, 140}
\definecolor{RedViolet}{RGB}{160, 34, 108}
\definecolor{finegray}{RGB}{248, 248, 245}
\definecolor{magenta5}{RGB}{199, 42, 199}
\definecolor{Emerald}{RGB}{0, 169, 159}
\definecolor{DeepPink}{RGB}{255, 20, 147}

\definecolor{structurecolor}{RGB}{0, 174, 247}
\definecolor{main}{RGB}{101, 238, 222}
\definecolor{second}{RGB}{244, 105, 102}
\definecolor{third}{RGB}{0, 174, 247}

% corlor definition
\definecolor{winered}{RGB}{212, 51, 131}
\colorlet{coverlinecolor}{second}

% ----- Title Style -----
\ifdefstring{\COLORFUL@titlestyle}{hang}{\def\style{hang}}{\relax}
\ifdefstring{\COLORFUL@titlestyle}{display}{\def\style{display}}{\relax}

% reference: 
% https://tex.stackexchange.com/questions/58506/how-to-make-a-new-command-similar-to-author
% https://pastebin.com/C8W4axzV
\newcommand\email[1]{\href{mailto:#1}{\nolinkurl{#1}}}

\global\let\@guide\@empty
\global\let\@title\@empty
\global\let\@author\@empty
\global\let\@institute\@empty
\global\let\@version\@empty
\global\let\@date\@empty
\newcommand{\guide}[1]{\gdef\@guide{#1}}
\newcommand{\subtitle}[1]{\gdef\@subtitle{#1}}
\newcommand{\institute}[1]{\gdef\@institute{#1}}
\newcommand{\version}[1]{\gdef\@version{#1}}
\newcommand{\extrainfo}[1]{\gdef\@extrainfo{#1}}

\RequirePackage{mwe}
\newcommand{\cover}[1]{\gdef\@cover{#1}}

\newcommand{\question}[1]{{\par\citshape #1}\\[0.2ex]}

\RequirePackage{enumerate}
% list/itemize/enumerate setting
\RequirePackage[shortlabels,inline]{enumitem}
\setlist{nolistsep}

\RequirePackage[labelfont={bf,color=structurecolor}]{caption} 
\captionsetup[table]{skip=3pt,labelsep=space}
\captionsetup[figure]{skip=3pt,labelsep=space}

\AtBeginDocument{
  \setlength{\abovedisplayskip}{3pt}
  \setlength{\belowdisplayskip}{3pt}
  \RequirePackage[flushmargin,stable]{footmisc}
  \setlength{\footnotesep}{12pt}
}

\RequirePackage{graphicx}
%\RequirePackage{enumerate}
\RequirePackage{amsmath,mathrsfs,amsfonts,amssymb}
% \providecommand\qed{}
% \renewcommand{\qed}{\hfill\ensuremath{\square}}
\RequirePackage{booktabs}
\RequirePackage{multicol,multirow}

\RequirePackage{fancyvrb}
\RequirePackage{makecell,lipsum,hologo}
%%中文结构名字

%%   章节设置
\RequirePackage[center,pagestyles]{titlesec}
\RequirePackage[title,titletoc,header]{appendix}

\RequirePackage[
  backend=\COLORFUL@bibend,
  citestyle=\COLORFUL@citestyle,
  bibstyle=\COLORFUL@bibstyle]{biblatex}

\ifdefstring{\COLORFUL@lang}{cn}{
  \renewcommand{\baselinestretch}{1.3}
  \renewcommand{\contentsname}{目\quad 录}
  \renewcommand{\figurename}{图}
  \renewcommand{\tablename}{表}
  \renewcommand{\partname}{\color{structurecolor}}
  \renewcommand{\thepart}{第\zhnumber{\arabic{part}}部分}
  \renewcommand{\listfigurename}{插图目录}
  \renewcommand{\listtablename}{表格目录}
  \renewcommand{\bibname}{参考文献}
  \newcommand{\ebibname}{参考文献}
  \renewcommand{\appendixname}{附录}
  \renewcommand{\appendixtocname}{附录}
  \renewcommand{\indexname}{索\hspace{2em}引}
  \newcommand\figref[1]{\textbf{图}~\ref{#1}}
  \newcommand\tabref[1]{\textbf{表}~\ref{#1}}
  \newcommand{\authorname}{\citshape \textbf{作者：}}
  \newcommand{\institutename}{\citshape \textbf{组织：}}
  \newcommand{\datename}{\citshape \textbf{时间：}}
  \newcommand{\versionname}{\citshape \textbf{版本：}}
  \newcommand{\guidename}{\citshape \textbf{指导：}}
  \newcommand{\notename}{笔记}
  \renewcommand*{\proofname}{证明}
  \newcommand{\definitionname}{定义}
  \newcommand{\theoremname}{定理}
  \newcommand{\axiomname}{公理}
  \newcommand{\postulatename}{公设}
  \newcommand{\lemmaname}{引理}
  \newcommand{\propositionname}{命题}
  \newcommand{\corollaryname}{推论}
  \newcommand{\examplename}{例题}
  \newcommand{\instancename}{示例}
  \newcommand{\problemname}{问题} % 问题
  \newcommand{\exercisename}{练习} % 练习=习题
  \newcommand{\remarkname}{注}
  \newcommand{\assumptionname}{假设}
  \newcommand{\conclusionname}{结论}
  \newcommand{\solutionname}{解}
  \newcommand{\propertyname}{性质}
  \newcommand{\introductionname}{内容提要}
  \newcommand{\introductionsname}{内容概述}
  \newcommand\bioinfo[2]{\gdef\@bioinfo{{\citshape #1}：#2}}
  \newcommand{\updatename}{更新：}
  \newcommand{\historyname}{版本更新历史}
  \newcommand{\beforechap}{第}
  \newcommand{\afterchap}{章}
}{\relax}

\RequirePackage{tikz} %% load tikz without tikz
\usetikzlibrary{backgrounds,calc,shadows,positioning,fit}
\newcommand*\circled[1]{\tikz[baseline=(char.base)]{
            \node[shape=circle,draw,inner sep=1pt] (char) {#1};}}
%  https://tex.stackexchange.com/questions/7032/good-way-to-make-textcircled-numbers

\newcommand*{\eitemi}{\tikz \draw [baseline, ball color=structurecolor,draw=none] circle (2pt);}
\newcommand*{\eitemii}{\tikz \draw [baseline, fill=structurecolor,draw=none,circular drop shadow] circle (2pt);}
\newcommand*{\eitemiii}{\tikz \draw [baseline, fill=structurecolor,draw=none] circle (2pt);}
\setlist[enumerate,1]{label=\color{structurecolor}\arabic*.}
\setlist[enumerate,2]{label=\color{structurecolor}(\alph*).}
\setlist[enumerate,3]{label=\color{structurecolor}\Roman*.}
\setlist[enumerate,4]{label=\color{structurecolor}\Alph*.}
\setlist[itemize,1]{label={\eitemiii}}
\setlist[itemize,2]{label={\eitemii}}
\setlist[itemize,3]{label={\eitemiii}}

\RequirePackage{apptools}
% appendix chapter:
\ifdefstring{\COLORFUL@lang}{cn}{
  \ifdefstring{\COLORFUL@scheme}{chinese}{
    \newcommand{\xchaptertitle}{第\zhnumber{\arabic{chapter}}章} }{
    \newcommand{\xchaptertitle}{第 \thechapter{} 章}}
% Hyperref 2022/05/16's changelog.txt said
% * nameref is loaded always and earlier (issue#235) 
    \AtBeginDocument{
      \ifCOLORFUL@Hy@later
        \patchcmd{\NR@chapter}%
          {\numberline{\thechapter}}{\numberline{\xchaptertitle}}%
          {}{\fail}
      \else    
        \patchcmd{\Hy@org@chapter}%
          {\numberline{\thechapter}}{\numberline{\xchaptertitle}}%
          {}{\fail}
      \fi
    }
  }
  {\newcommand{\xchaptertitle}{\chaptername~\thechapter~}}

\setcounter{secnumdepth}{5}
\titleformat{\chapter}[\style]{\bfseries}{
  \filcenter\LARGE\enspace\bfseries{\color{structurecolor} \IfAppendix{\appendixname\;\thechapter\;}{\xchaptertitle\;}}}{1pt}{
  \LARGE\bfseries\color{structurecolor}\filcenter}[]
\titleformat{\section}[hang]{\bfseries}{
  \Large\bfseries{\color{structurecolor}\thesection}\enspace}{1pt}{%
  \color{structurecolor}\Large\bfseries\filright}
\titleformat{\subsection}[hang]{\bfseries}{
  \large\bfseries\color{structurecolor}\thesubsection\enspace}{1pt}{%
  \color{structurecolor}\large\bfseries\filright}
\titleformat{\subsubsection}[hang]{\bfseries}{
  \large\bfseries\color{structurecolor}\thesubsubsection\enspace}{1pt}{%
  \color{structurecolor}\large\bfseries\filright}

\titlespacing{\chapter}{0pt}{-20pt}{1.3\baselineskip}

%% define the 
%% definition theorem postulate axiom corollary lemma proposition 
%% and proof environment
\RequirePackage{pifont,manfnt,bbding}
\RequirePackage[many]{tcolorbox}
% \newlength{\normalparindent}
% \setlength{\normalparindent}{\parindent}
\definecolor{axiomcolor}{RGB}{255, 134, 24}%
\ifdefstring{\COLORFUL@mode}{fancy}{
  \tcbset{
    common/.style={
      fontupper=\citshape,
      lower separated=false,
      % before upper={\setlength{\parindent}{\normalparindent}},
      coltitle=black,
      colback=gray!5,
      boxrule=0.5pt,
      fonttitle=\bfseries,
      enhanced,
      breakable,
      top=8pt,
      before skip=8pt,
      attach boxed title to top left={
        yshift=-0.11in,
        xshift=0.15in},
      boxed title style={
        boxrule=0pt,
        colframe=white,
        arc=0pt,
        outer arc=0pt},
      separator sign={.},},
    defstyle/.style={  %定义
      common,
      colframe=main,  
      colback=main!5,
      colbacktitle=main, 
      overlay unbroken and last={
        \node[anchor=south east, outer sep=0pt] at (\linewidth-width,0) {
          \textcolor{main}{$\clubsuit$}};}},
    thmstyle/.style={ %定理
      common,
      coltitle=white,
      colframe=second,  
      colback=second!5,
      colbacktitle=second, 
      overlay unbroken and last={
        \node[anchor=south east, outer sep=0pt] at (\linewidth-width,0) {
          \textcolor{second}{$\heartsuit$}};}},
    axiostyle/.style={ %公理
      common,
      coltitle=white,
      colframe=cyan,  
      colback=cyan!5,
      colbacktitle=cyan, 
      overlay unbroken and last={
        \node[anchor=south east, outer sep=0pt] at (\linewidth-width,0) {
          \textcolor{pink}{$\spadesuit$}};}},
    prostyle/.style={ %公设
      common,
      colframe=violet,
      coltitle=white,
      colback=violet!5,
      colbacktitle=violet!80, 
      overlay unbroken and last={
        \node[anchor=south east, outer sep=0pt] at (\linewidth-width,0) {
          \textcolor{violet}{$\spadesuit$}};}},
    propostyle/.style={ %命题
      common,
      colframe=blue,
      coltitle=white,
      colback=blue!5,
      colbacktitle=blue!80, 
      overlay unbroken and last={
        \node[anchor=south east, outer sep=0pt] at (\linewidth-width,0) {
          \textcolor{blue}{$\spadesuit$}};}},
    corostyle/.style={ %推论
      common,
      colframe=magenta!60,
      coltitle=white,
      colback=magenta!5,
      colbacktitle=magenta!80, 
      overlay unbroken and last={
        \node[anchor=south east, outer sep=0pt] at (\linewidth-width,0) {
          \textcolor{magenta}{$\spadesuit$}};}},
    lemostyle/.style={ %引理
      common,
      colframe=orange,
      coltitle=white,
      colback=orange!5,
      colbacktitle=orange!70, 
      overlay unbroken and last={
        \node[anchor=south east, outer sep=0pt] at (\linewidth-width,0) {
          \textcolor{orange}{$\spadesuit$}};}},
    COLORFUL@title/.code n args={2}
      {
        \ifblank{#2}
          {\tcbset{title={\csname #1name\endcsname~\thetcbcounter}}}
          {\tcbset{title={\csname #1name\endcsname~\thetcbcounter\ (#2)}}}
      },
    COLORFUL@label/.code n args={2}
      {
        \ifblank{#2}
          {}{\tcbset{label={#1:#2}}}
      }
    }
  
  % define an internal control sequence \COLORFUL@newtheorem for fancy mode's newtheorem
  % #1 is the environment name, #2 is the prefix of label, #3 is the style
  % style: thmstyle, defstyle, prostyle
  % e.g. \COLORFUL@newtheorem{theorem}{thm}{thmstyle}
  % will define two environments: numbered ``theorem'' and no-numbered ``theorem*''
  % WARNING FOR MULTILINGUAL: this cs will automatically find \theoremname's definition,
  % WARNING FOR MULTILINGUAL: it should be defined in language settings.
  \newcommand{\COLORFUL@newtheorem}[3]{
    \ifcsundef{#1name}{%
      \ClassError{COLORFULbook}{%
        \ #1name undefined, \MessageBreak
        Please check in language setting
      }{}
    }{\relax}
    \DeclareTColorBox[auto counter,number within=\COLORFUL@thmcnt]{#1}{ g o t\label g }{
        common,#3,
        IfValueTF={##1}
          {COLORFUL@title={#1}{##1}}
          {
            IfValueTF={##2}
            {COLORFUL@title={#1}{##2}}
            {COLORFUL@title={#1}{}}
          },
        IfValueT={##4}
          {
            IfBooleanTF={##3}
              {label={##4}}
              {COLORFUL@label={#2}{##4}}
          }
      }
    \DeclareTColorBox{#1*}{ g o }{
        common,#3,
        IfValueTF={##1}
          {COLORFUL@title={#1}{##1}}
          {
            IfValueTF={##2}
            {COLORFUL@title={#1}{##2}}
            {COLORFUL@title={#1}{}}
          },
      }
  }
  % define several environment 
  % we define headers like \definitionname before
  \COLORFUL@newtheorem{definition}{def}{defstyle}
  \COLORFUL@newtheorem{theorem}{thm}{thmstyle}
  \COLORFUL@newtheorem{postulate}{pos}{prostyle} % prostyle
  \COLORFUL@newtheorem{axiom}{axi}{axiostyle}
  \COLORFUL@newtheorem{corollary}{cor}{corostyle}
  
  \COLORFUL@newtheorem{lemma}{lem}{lemostyle}
  \COLORFUL@newtheorem{proposition}{pro}{propostyle}
}{\relax}


\ifdefstring{\COLORFUL@mode}{simple}{
  \let\openbox\relax
  \RequirePackage{amsthm}
  \let\proof\relax
  % \let\proofname\relax
  \let\endproof\relax

  % declare new theorem styles: defstyle, thmstyle, prostyle
  \newtheoremstyle{defstyle}{3pt}{3pt}{\citshape}{-3pt}{
    \bfseries\color{main}}{}{0.5em}{\thmname{#1} \thmnumber{#2} \thmnote{(#3)}}
  \newtheoremstyle{thmstyle}{3pt}{3pt}{\citshape}{-3pt}{
    \bfseries\color{second}}{}{0.5em}{\thmname{#1} \thmnumber{#2} \thmnote{(#3)}}
  \newtheoremstyle{prostyle}{3pt}{3pt}{\citshape}{-3pt}{
    \bfseries\color{third}}{}{0.5em}{\thmname{#1} \thmnumber{#2} \thmnote{(#3)}}

  % define an internal control sequence \COLORFUL@newtheorem for simple mode's newtheorem
  % #1 is the environment name, #2 is the style
  % style: thmstyle, defstyle, prostyle
  % e.g. \COLORFUL@newtheorem{theorem}{thmstyle}
  % will define two environments: numbered ``theorem'' and no-numbered ``theorem*''
  % WARNING FOR MULTILINGUAL: this cs will automatically find \theoremname's definition,
  % WARNING FOR MULTILINGUAL: it should be defined in language settings.
  \newcommand{\COLORFUL@newtheorem}[2]{
    \theoremstyle{#2}
    \newtheorem{#1}{\csname #1name\endcsname}[\COLORFUL@thmcnt]
    \newtheorem*{#1*}{\csname #1name\endcsname}
  }

  % define several environment 
  % we define headers like \definitionname before
  \COLORFUL@newtheorem{definition}{defstyle}
  \COLORFUL@newtheorem{theorem}{thmstyle}
  \COLORFUL@newtheorem{postulate}{prostyle}
  \COLORFUL@newtheorem{axiom}{axiostyle}
  \COLORFUL@newtheorem{corollary}{thmstyle}
  \COLORFUL@newtheorem{lemma}{thmstyle}
  \COLORFUL@newtheorem{proposition}{prostyle}
}{\relax}

% define an user control sequence \COLORFULnewtheorem
% #1 is the evironment, #2 is the theorem header
% #3 is the style       #4 is the prefix for fancy mode
% style: thmstyle, defstyle, prostyle
% if #4 is given in simple mode, an error will be raised
% if #4 isn't given in fancy mode, the prefix will be set equal to #1,
%   and a warning will be raised
\newcommand{\COLORFUL@newtheorem@warning}{}
\newcommand{\COLORFUL@newtheorem@error}{}
\NewDocumentCommand\COLORFULnewtheorem{ m m m g }
  {
    \renewcommand{\COLORFUL@newtheorem@warning}{
      \ClassWarning{COLORFULbook}{%
        Because you didn't provide a prefix. \MessageBreak
        We use #1 as the default prefix. \MessageBreak
        You have to use \MessageBreak
        \ref{#1:label} to refer a \MessageBreak
        \begin{#1}{name}{label} environment. \MessageBreak
      }
    }
    \renewcommand{\COLORFUL@newtheorem@error}{
      \ClassError{COLORFULbook}{%
        You can't set a prefix in mode ``simple''.\MessageBreak
        Just use \MessageBreak
        \COLORFULnewtheorem{#1}{#2}{#3}
      }{%
        We are using ``amsthm'' package to \MessageBreak
        generate theorem-like theorem. \MessageBreak
        The 4th parameter ``prefix'' isn't allowed.
      }
    }
    \expandafter\def\csname #1name\endcsname{#2}
    \ifdefstring{\COLORFUL@mode}{simple}{%
        \IfValueTF{#4}
          {\COLORFUL@newtheorem@error}
          {\COLORFUL@newtheorem{#1}{#3}}
      }{\relax}
    \ifdefstring{\COLORFUL@mode}{fancy}{%
        \IfValueTF{#4}
          {
            \def\COLORFUL@temp@prefix{#4}
            \ifdefempty{\COLORFUL@temp@prefix}
              {
                \COLORFUL@newtheorem{#1}{#1}{#3}
                \COLORFUL@newtheorem@warning
              }
              {\COLORFUL@newtheorem{#1}{#4}{#3}}
          }
          {
            \COLORFUL@newtheorem{#1}{#1}{#3}
            \COLORFUL@newtheorem@warning
          }
      }{\relax}
  }

% main（green-def): example exercise problem solution
% second（orange-thm）: proof note remark  
% third（blue-prop):  assumptions property conclusion custom
%% Example with counter  % 手动注释
%\newcounter{exam}[chapter]
%\setcounter{exam}{0}
%\renewcommand{\theexam}{\thechapter.\arabic{exam}}
%\newenvironment{example}[1][]{
%  \refstepcounter{exam}
%  \par\noindent\textbf{\color{main}{\examplename} \theexam #1 }\rmfamily}{
%  \par\ignorespacesafterend}

%% Exercise with counter
\newcounter{exer}[chapter]
\setcounter{exer}{0}
\renewcommand{\theexer}{\thechapter.\arabic{exer}}
\newenvironment{exercise}[1][]{
  \refstepcounter{exer}
  \par\noindent\makebox[-3pt][r]{
    \scriptsize\color{red!90}\HandPencilLeft\quad}
    \textbf{\color{main}{\exercisename} \theexer #1 }\rmfamily}{
    \par\ignorespacesafterend}

%% Problem with counter
\newcounter{prob}[chapter]
\setcounter{prob}{0}
\renewcommand{\theprob}{\thechapter.\arabic{prob}}
\newenvironment{problem}[1][]{
  \refstepcounter{prob}
  \par\noindent\textbf{\color{main}{\problemname} \theprob #1 }\rmfamily}{
  \par\ignorespacesafterend}

\newenvironment{note}{%
  \smallskip
  \begin{anybox}{\notename}%
}{\end{anybox}}

\newenvironment{proof}{
  \par\noindent\textbf{\color{second}\proofname\;}
  \color{black!90}\cfs}{
  % \hfill$\Box$\quad
  \par}

\newenvironment{solution}{\par\noindent\textbf{\color{main}\solutionname} \citshape}{\par}
\newenvironment{remark}{%
  \smallskip
  \begin{anybox}{\remarkname}%
}{\end{anybox}}
\newenvironment{custom}[1]{\par\noindent\textbf{\color{third} #1} \citshape}{\par}

\RequirePackage{multicol}
\tcbset{
  introductionsty/.style={
    enhanced,
    breakable,
    colback=structurecolor!10,
    colframe=structurecolor,
    fonttitle=\bfseries,
    colbacktitle=structurecolor,
    fontupper=\citshape,
    attach boxed title to top center={yshift=-3mm,yshifttext=-1mm},
    boxrule=0pt,
    toprule=0.5pt,
    bottomrule=0.5pt,
    top=8pt,
    before skip=8pt,
    sharp corners
  },
}

\newenvironment{introduction}[1][\introductionname]{
  \begin{tcolorbox}[introductionsty,title={#1}]
    \begin{multicols}{2}
      \begin{itemize}[label=\textcolor{structurecolor}{\upshape\scriptsize\SquareShadowBottomRight}]}{
      \end{itemize}
    \end{multicols}
  \end{tcolorbox}}

\tcbset{
    alphasty/.style={
        enhanced,
        breakable,
        colback=azures1,
        colframe=azures2,
        boxrule=0pt,
        top=8pt,
        before skip=12pt,
        sharp corners,
        arc=0mm,
        drop lifted shadow={color=black!20!white,opacity=.3,shadow xshift=0.1cm},%盒子阴影
        overlay = {%
            %标题盒子
            \node[rectangle, %opacity=.3,
            text=white, drop shadow={opacity=.3, shadow xshift=0.1cm},rounded corners=2pt,
            inner sep=1.5mm,fill=azures2,
            anchor=west,
            font=\bfseries] at ([xshift=.45\textwidth,yshift=.5mm]frame.north west)%
            {\bf \introductionsname};
        },
    }
}
%%--------------------------------------------------%%
\newenvironment{introductions}[1][\introductionsname]{\smallskip
    \begin{tcolorbox}[alphasty]
        \begin{multicols}{2}
            \begin{itemize}[label=\textcolor{structurecolor}{\upshape\scriptsize\SquareShadowBottomRight}]}{
            \end{itemize}
        \end{multicols}
    \end{tcolorbox}\smallskip}

%%%%%%%%%%%%%%%%%%
\RequirePackage{adforn}

\newenvironment{problemset}[1][\xchaptertitle~\exercisename]{
  \vspace*{10pt}
  \begin{center}
    \phantomsection\addcontentsline{toc}{section}{\texorpdfstring{\xchaptertitle~\exercisename}{\exercisename}}
    % \markboth{#1}{\rightmark}
    \markright{#1}
    \textcolor{structurecolor}{\Large\bfseries\adftripleflourishleft~#1~\adftripleflourishright}
  \end{center}
  \begin{enumerate}}{
  \end{enumerate}}

\def\relsec{\endgroup start}
\def\endrelsec{end\begingroup\def \@currenvir {relsec}}

\ifdefstring{\COLORFUL@result}{noanswer}{
  \AtBeginDocument{
  \excludecomment{solution}
  \excludecomment{proof}
  \excludecomment{inline}
  }
}{\relax}

%页眉页脚

%%%%%%%%%%%%%%%%%%%%%%%
\RequirePackage{fancyhdr}
\RequirePackage{fontawesome}
\RequirePackage{xhfill}
\fancyhf{}

\fancyfoot[c]{\color{structurecolor} \faGg 第~\color{black}{\thepage}\color{structurecolor}~页 \faGg}

\fancyhead[L]{\color{structurecolor}{\faPaperPlaneO}\cnormal\leftmark} % E表示偶数页
% \fancyhead[R]{ \color{structurecolor}\cnormal\rightmark}

\renewcommand{\headrule}{\color{orange!90}\hrule width\textwidth}
\pagestyle{fancy}

\fancypagestyle{plain}{\pagestyle{fancy}}

\renewcommand{\sectionmark}[1]{\markright{\thesection\, #1}{}}
\renewcommand{\chaptermark}[1]{\IfAppendix{\markboth{附录\Alph{chapter}\; #1}{}}{\markboth{\xchaptertitle\, #1}{}}}
%%%%%%%%%%%%%%%%%%%%%%%%%

\renewcommand*{\maketitle}{%
\hypersetup{pageanchor=false}
\pagenumbering{Alph}
\begin{titlepage}
  \newgeometry{margin = 0in}
  \parindent=0pt
  \ifdefstring{\COLORFUL@device}{normal}{
    \ifcsname @cover\endcsname
      \includegraphics[width=\linewidth]{\@cover}
    \else
      \includegraphics[width=\linewidth]{example-image}
    \fi
  }{\relax}
  \ifdefstring{\COLORFUL@device}{pad}{
    \ifcsname @cover\endcsname
      \includegraphics[trim=0 26bp 0 26bp,clip=true, width=\linewidth]{\@cover}
    \else
      \includegraphics[trim=0 26bp 0 26bp,clip=true, width=\linewidth]{example-image}
    \fi
  }{\relax}
  \setlength{\fboxsep}{0pt}
  \colorbox{coverlinecolor}{\makebox[\linewidth][c]{\shortstack[c]{\vspace{0.5in}}}}
  \vfill
  \vskip-2ex
  \hspace{2em}
  \parbox{0.9\textwidth}{
    \bfseries\LARGE 
      \ifcsname @title\endcsname \@title \fi
    \par}
  \vfill
  \vspace{-1.0cm}
  \setstretch{2.5}
  \hspace{2.5em}
  \begin{minipage}[c]{0.9\linewidth}
    {\color{darkgray}\bfseries\Large
      \ifcsname @subtitle\endcsname\@subtitle\\[2ex]\fi}
    \color{gray}\normalsize
    {\renewcommand{\arraystretch}{0.618}
    \begin{tabular}{l}
      \ifx\@author\empty\else\authorname\cnormal\@author\\ \fi
      \ifx\@institute\empty\else\institutename\cnormal\@institute\\ \fi
      \ifx\@date\empty\else\datename\cnormal\@date \\ \fi
      \ifx\@guide\empty\else\guidename\cnormal\@guide \\ \fi
      \ifx\@version\empty\else\versionname\cnormal\@version \\ \fi
      \ifcsname @bioinfo\endcsname \cnormal\@bioinfo\\ \fi
    \end{tabular}}
  \end{minipage}
  \vfill
  \begin{center}
    \setstretch{1.3}
    \parbox[t]{0.7\textwidth}{\centering \citshape 
      \ifcsname @extrainfo\endcsname\@extrainfo\fi}
  \end{center}
  \vfill
\end{titlepage}
\restoregeometry
\thispagestyle{empty}}

\newcommand{\dollar}{\mbox{\textdollar}}

\RequirePackage{listings}
\renewcommand{\ttdefault}{cmtt}
\lstdefinestyle{mystyle}{
  basicstyle=
    \ttfamily
    \lst@ifdisplaystyle\small\fi
}

\lstset{basicstyle=\ttfamily,style=mystyle,breaklines=true}

\definecolor{lightgrey}{rgb}{0.9, 0.9, 0.9}
\definecolor{frenchplum}{RGB}{190, 20, 83}
\lstset{language=[LaTeX]TeX,
  texcsstyle=*\color{winered},
  numbers=none,
  mathescape=false,
  breaklines=true,
  keywordstyle=\color{winered},
  commentstyle=\color{gray},
  emphstyle={\color{frenchplum}},
  frame=single,
  tabsize=2,
  rulecolor=\color{structurecolor},
  framerule=0.2pt,
  columns=flexible,
  frame=single % 边框
}

\newcommand\bmmax{0}
\RequirePackage{bm}

% add the list of change history
\newcommand{\dateinfoline}[2]{
  \noindent\makebox[0pt][r]{%
  \makebox[-3em][r]{%
    \small
    \textbf{\textcolor{structurecolor}{#1}}}\;\;}%
  {\citshape\updatename\ignorespaces#2}}

\newcommand{\datechange}[2]{%
  \noindent{\makebox[\textwidth][r]{\color{structurecolor}\rule{1.15\textwidth}{.4pt}}}
  \dateinfoline{#1}{#2}
  \addcontentsline{dates}{section}{#1 -- #2}}

\newcommand{\listofchanges}{%
  \begingroup
    \renewcommand{\contentsname}{\historyname}
    \let\old@starttoc\@starttoc
    \def\@starttoc##1{
      \old@starttoc{dates}}
    \tableofcontents%
  \endgroup}
% https://tex.stackexchange.com/questions/472931/list-of-all-dates-in-a-document

\newenvironment{change}{
  \begin{enumerate}[label=\small\protect\circled{\arabic*}]}{
  \end{enumerate}}

  \renewcommand\tableofcontents{%
  \hypersetup{linktoc=all, linkcolor=black}
    \if@twocolumn
      \@restonecoltrue\onecolumn
    \else
      \@restonecolfalse
    \fi
    \chapter*{\contentsname
        \@mkboth{%
           \MakeUppercase\contentsname}{\MakeUppercase\contentsname}}%
    \ifdefstring{\COLORFUL@toc}{twocol}{
      \setlength{\columnsep}{2em}
      \begin{multicols}{2}%
        \@starttoc{toc}
      \end{multicols}}{
      \@starttoc{toc}}
    \if@restonecol\twocolumn\fi
    \hypersetup{linkcolor=winered}}


\renewcommand*{\cleardoublepage}{\clearpage}


% https://tex.stackexchange.com/questions/56839/chaptername-is-used-even-for-appendix-chapters-in-toc
\RequirePackage{calc}
\RequirePackage[titles]{tocloft}
\ifdefstring{\COLORFUL@lang}{cn}{
  % \renewcommand{\cftchappresnum}{\beforechap\space}
  % \renewcommand{\cftchapaftersnum}{\space\afterchap}
  \setlength{\cftchapnumwidth}{\widthof{\textbf{附录~999}}}
  \g@addto@macro\appendix{%
    % recover \thechapter in the \NR@chapter's \numberline
    \ifCOLORFUL@Hy@later
      \patchcmd{\NR@chapter}%
        {\numberline{\xchaptertitle}}{\numberline{\thechapter}}%
        {}{\fail}      
    \else
      \patchcmd{\Hy@org@chapter}%
        {\numberline{\xchaptertitle}}{\numberline{\thechapter}}%
        {}{\fail}  
    \fi
    \addtocontents{toc}{%
      \protect\renewcommand{\protect\cftchappresnum}{\appendixname\space}%
      \protect\renewcommand{\protect\cftchapaftersnum}{}%
    }%
  }
}{
  \renewcommand{\cftchappresnum}{\chaptername\space}
  \renewcommand{\cftchapaftersnum}{\space}
  \setlength{\cftchapnumwidth}{\widthof{\textbf{Appendix~9}}}
  \g@addto@macro\appendix{%
    \addtocontents{toc}{%
      \protect\renewcommand{\protect\cftchappresnum}{\appendixname\space}%
      \protect\renewcommand{\protect\cftchapaftersnum}{}%
      \setlength{\cftchapnumwidth}{\widthof{\textbf{Appendix~999}}}
    }%
  }
}

% restore the tt default family to lmodern tt family
\renewcommand\ttdefault{lmtt}

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\RequirePackage{fontawesome}
\RequirePackage{wallpaper}
\RequirePackage{xhfill}
\RequirePackage[object=vectorian]{pgfornament} 
%%%%%%%%%%%%%%%%listings定制%%%%%%%%%%%%%%%%%%%
\RequirePackage{tcolorbox}
\tcbuselibrary{listings,breakable}

\newcommand{\md}[1]{{\color{purple}#1}}
\newcommand{\keypoint}[1]{\textbf{\textcolor{red}{#1}}}
% \definecolor{structurecolor}{RGB}{0, 174, 247}%

\RequirePackage{listings}
\renewcommand{\ttdefault}{cmtt}
\lstdefinestyle{mystyle}{
  basicstyle=%
    \ttfamily
    \lst@ifdisplaystyle\small\fi,
  emphstyle={\color{cyan}},
  breakatwhitespace=true,
	rulecolor=\color{structurecolor},
	columns=flexible,
  numbers=left,
  breaklines=true,
	texcsstyle=*\color{red!40!orange},
	keywordstyle=\color{red!40!orange},
	commentstyle=\color{gray},
  escapeinside=``,
  rulesepcolor=\color[RGB]{0,128,128},
  stringstyle=\color[RGB]{128,0,0},
  showstringspaces=false,
  backgroundcolor=\color{gray!5},
  framerule=1pt,
  frame=tb,
  numbers=left,
  numberstyle=\color{black},
  escapeinside={(*@}{@*)}
}

\lstset{basicstyle=\ttfamily,style=mystyle}

\definecolor{lightgrey}{rgb}{0.9, 0.9, 0.9}
\definecolor{frenchplum}{RGB}{190, 20, 83}
\lstset{language=[LaTeX]TeX}

% 实验报告横线占位宏，支持可选长度参数。
\newcommand{\LabFillLine}[1][4cm]{\rule{#1}{0.4pt}}
\newcommand{\LabFillBlack}[1][4cm]{\hspace{0.4pt}}
% 图片或关键代码粘贴框：标题、填写提示和图标签分别作为三个参数。
\newcommand{\LabPasteBox}[3]{%
  \begin{figure}[htbp]\centering
    \fbox{请在此粘贴图片}%
    \caption{#1：#2}\label{#3}%
  \end{figure}%
}

% 优先使用 CMU 等宽字体；在精简 TeX 环境中回退到 Latin Modern Mono，避免找不到字体导致 XeLaTeX 中止。
\IfFontExistsTF{CMU Typewriter Text}{%
  \setmonofont{CMU Typewriter Text}%
}{%
  \IfFontExistsTF{Latin Modern Mono}{%
    \setmonofont{Latin Modern Mono}%
  }{%
    \setmonofont{DejaVu Sans Mono}%
  }%
}

%%%%%%%%%%%%%%%%%性质%%%%%$%%%%%%%%%%%
\newcounter{property}[section]
\setcounter{property}{0}
\renewcommand{\theproperty}{\arabic{property}}

\newtcolorbox{wwproposition}[2][]{
    arc=0mm,breakable,enhanced,colback=lightergray,boxrule=0pt,top=8mm,drop fuzzy shadow=black!20!white,
    fontupper=\normalsize, 
    overlay unbroken = {
        %左上边缘细线%更改有效
        % \draw[color=gray,  line width=0.2pt] (frame.north west)--([xshift=0pt]frame.north east);
        % \draw[color=gray,  line width=0.2pt] (frame.south west)--([xshift=0pt]frame.south east);
        %标题%更改有效
        \node[rectangle, %opacity=.3,
        text=black, drop shadow={opacity=.3, shadow xshift=0.1cm},
        inner sep=1.5mm,fill=structurecolor,
        anchor=west,
        font=\normalsize] at ([xshift=0cm,yshift=-3.mm]frame.north west)%
        {\bf  \color{white} \propertyname~\theproperty. ~#2}; %这里可以补计数器
        %底部 (下边缘)颜色装饰矩形%更改有效
        \fill[fill=structurecolor]  (.9\textwidth,0.2cm) rectangle(\textwidth,-0.0cm);
    },%上面是实际控制代码,下面的代码仅在跨页分割时生效
    overlay first = {%跨页时生效
        % \draw[color=gray,  line width=0.2pt] (frame.north west)--([xshift=0pt]frame.north east);
        %标题 %跨页时生效
        \node[rectangle, %opacity=.3,
        text=black, drop shadow={opacity=.3, shadow xshift=0.1cm},
        inner sep=1.5mm,fill=DeepPink,
        anchor=west,
        font=\normalsize] at ([xshift=0cm,yshift=-3.mm]frame.north west)%
        {\bf  \color{white}  \propertyname~\theproperty. #2};
    },%保持边缘的变化
    overlay last = {%跨页时生效
        % \draw[color=gray,  line width=0.2pt] (frame.south west)--([xshift=0pt]frame.south east);
        %底部%跨页时生效
        \fill[fill=structurecolor]  (.9\textwidth,0.2cm) rectangle(\textwidth,-0.0cm);
    }
    #1
}
\NewDocumentEnvironment{property}{O{} O{} O{}}{\stepcounter{property} \smallskip\begin{wwproposition}{#1}{#2}#3}{\end{wwproposition} }

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\tikzset{
    wnodeTheorem/.style={%
        rectangle,top color=lightergray, bottom color=lightergray,
        inner sep=1mm,anchor=west,font=\bf\rmfamily},
    % wnodeminimum/.style={%
        % 	rectangle,  top color=white, bottom color=white,
        % 	text=Emerald,inner sep=1mm,anchor=west,font=\bf\rmfamily}
}
%%%%%%%%%%%%%%%%评析环境%%%%%%%%%%%%%%%%
\newtcolorbox{anybox}[2][]{%
    arc=0mm,breakable,enhanced,colback=lightergray,boxrule=0pt,
    top=6mm,fontupper=\normalsize,%step and label={tcbremark}{#3},
    overlay unbroken  = {
        %垂直条%更改有效
        \draw[color=magenta5,  line width=2pt] ([xshift=2pt] frame.north west)--([xshift=2pt] frame.south west);
        %标题：定义%更改有效
        \node[wnodeTheorem](title) at ([xshift=4.5pt, yshift=-3mm]frame.north west)
        {\textbf{\color{magenta5} #2}};
        % \draw[color=magenta5,line width=2pt] ([xshift=3.5pt,yshift=-5.5mm] frame.north west)--([yshift=-5.5mm] frame.north east);
    }, %overlay
    overlay first  = {
        %垂直线段%跨页时生效
        \draw[color=magenta5,  line width=2pt] ([xshift=2pt] frame.north west)--([xshift=2pt] frame.south west);
        %标题:定义 %跨页时生效
        \node[wnodeTheorem](title) at ([xshift=4.5pt, yshift=-3mm]frame.north west)
        {\textbf{\color{magenta5} #2}};
        % \draw[color=magenta5,line width=2pt] ([xshift=3.5pt,yshift=-5.5mm] frame.north west)--([yshift=-5.5mm] frame.north east);
    }, %overlay
    % 保持边缘的变化
    overlay last    = {
        %垂直线段%跨页时生效
        \draw[color=magenta5,  line width=2pt] ([xshift=2pt] frame.north west)--([xshift=2pt] frame.south west);
    },
    #1
}
%
\NewDocumentEnvironment{anymark}{O{} O{} O{}}{\smallskip\begin{anybox}{#1}{#2} #3}{\end{anybox}}
% 结束评析
% 例题开始 --------------------------------------------------
% Example with counter
\newcounter{exam}[chapter]
\setcounter{exam}{0}
\renewcommand{\theexam}{\thechapter.\arabic{exam}}
% 例题环境
\newtcolorbox{wwexample}[3][]{%
    arc=0mm,
    breakable,drop fuzzy shadow=black!20!white,
    enhanced,
    colback=lightergray,
    boxrule=0pt,
    top=8mm, %分隔垂直-开始文本
    % enlarge top by=\baselineskip/2+1mm,
    % enlarge top at break by=0mm,pad at break=2mm,
    fontupper=\normalsize,
    step and label={exam}{#3},%标签
    overlay unbroken = {%
        %垂直线段%更改有效
        %\draw[color=gray!5,line width=3pt] ([xshift=2pt] frame.north west)--([xshift=2pt] frame.south west);
        %底部装饰矩形%更改有效
        \fill[fill=Emerald] (.9\textwidth,0.2cm) rectangle(\textwidth,-0.0cm);
        % 图像框示例
        %\node[rectangle,
        %         text=black, ,fill=ALamarillo!20,
        %         inner sep=1mm,anchor=west,font=\small\bf\sffamily] at ([xshift=-10.3pt,yshift=-3.2mm]frame.north west)%
        %{ };
        %框编号和描述%更改有效
        \node[rectangle, %opacity=.3,
        text=black, drop shadow={opacity=.3, shadow xshift=0.1cm},
        inner sep=1.5mm,fill=Emerald,
        anchor=west,
        font=\normalsize] at ([xshift=0cm,yshift=-3.mm]frame.north west)%
        {\bf  \color{black}例题 \theexam. ~#2};%标题
    },
    % fin overlay
    overlay first  = {%
        %标题框编号和描述%跨页时生效
        \node[rectangle, %opacity=.3,
        text=black, drop shadow={opacity=.3, shadow xshift=0.1cm},
        inner sep=1.5mm,fill=Emerald,
        anchor=west,
        font=\normalsize] at ([xshift=0cm,yshift=-3.mm]frame.north west)%
        {\bf \color{white} 例题 \theexam.~#2};
    },
    % overlay
    %边缘改变%跨页时生效
    overlay middle={\draw[color=lightergray,line width=3pt] ([xshift=3pt] frame.north west)--([xshift=2pt] frame.south west);
    },
    overlay last={\draw[color=lightergray,line width=3pt] ([xshift=3pt] frame.north west)--([xshift=2pt] frame.south west);
        %底部装饰矩形%跨页时生效
        \fill[fill=Emerald] (.9\textwidth,0.2cm) rectangle(\textwidth,-0.0cm); }
    #1}
%
\NewDocumentEnvironment{example}{O{} O{} O{}}{\smallskip\begin{wwexample}{#1}{#2}%
        #3}{\end{wwexample} }
%%%%%%%%%%%%%%%%例题结束%%%%%%%%%%%%%%%%

\newcounter{assumption}
\setcounter{assumption}{1}
\renewcommand{\theassumption}{\arabic{assumption}}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\newtcolorbox{wproposition}[2][]{
    arc=0mm,breakable,enhanced,colback=lightergray,boxrule=0pt,top=8mm,drop fuzzy shadow=black!20!white,
    fontupper=\normalsize, 
    overlay unbroken = {
        %左上边缘细线%更改有效
        % \draw[color=gray,  line width=0.2pt] (frame.north west)--([xshift=0pt]frame.north east);
        % \draw[color=gray,  line width=0.2pt] (frame.south west)--([xshift=0pt]frame.south east);
        %标题%更改有效
        \node[rectangle, %opacity=.3,
        text=black, drop shadow={opacity=.3, shadow xshift=0.1cm},
        inner sep=1.5mm,fill=DeepPink,
        anchor=west,
        font=\normalsize] at ([xshift=0cm,yshift=-3.mm]frame.north west)%
        {\bf  \color{white} 假设~\theassumption. ~#2}; %这里可以补计数器
        %底部 (下边缘)颜色装饰矩形%更改有效
        \fill[fill=DeepPink]  (.9\textwidth,0.2cm) rectangle(\textwidth,-0.0cm);
    },%上面是实际控制代码,下面的代码仅在跨页分割时生效
    overlay first = {%跨页时生效
        % \draw[color=gray,  line width=0.2pt] (frame.north west)--([xshift=0pt]frame.north east);
        %标题 %跨页时生效
        \node[rectangle, %opacity=.3,
        text=black, drop shadow={opacity=.3, shadow xshift=0.1cm},
        inner sep=1.5mm,fill=DeepPink,
        anchor=west,
        font=\normalsize] at ([xshift=0cm,yshift=-3.mm]frame.north west)%
        {\bf  \color{white}  假设~\theassumption. #2};
    },%保持边缘的变化
    overlay last = {%跨页时生效
        % \draw[color=gray,  line width=0.2pt] (frame.south west)--([xshift=0pt]frame.south east);
        %底部%跨页时生效
        \fill[fill=DeepPink]  (.9\textwidth,0.2cm) rectangle(\textwidth,-0.0cm);
    }
    #1
}
\NewDocumentEnvironment{assumption}{O{} O{} O{}}{\smallskip\begin{wproposition}{#1}{#2}#3}{\end{wproposition} }

\newtcolorbox{keypointbox}[2][]{%
    arc=0mm,breakable,enhanced,colback=lightergray,boxrule=0pt,
    top=6mm,fontupper=\normalsize,%step and label={tcbremark}{#3},
    overlay unbroken  = {
        %垂直条%更改有效
        \draw[color=structurecolor, line width=2pt] ([xshift=2pt] frame.north west)--([xshift=2pt] frame.south west);
        %标题：定义%更改有效
        \node[wnodeTheorem](title) at ([xshift=4.5pt, yshift=-3mm]frame.north west)
        {\textbf{\color{magenta5} 性质~#2}};
        % \draw[color=magenta5,line width=2pt] ([xshift=3.5pt,yshift=-5.5mm] frame.north west)--([yshift=-5.5mm] frame.north east);
    }, %overlay
    overlay first  = {
        %垂直线段%跨页时生效
        \draw[color=structurecolor, line width=2pt] ([xshift=2pt] frame.north west)--([xshift=2pt] frame.south west);
        %标题:定义 %跨页时生效
        \node[wnodeTheorem](title) at ([xshift=4.5pt, yshift=-3mm]frame.north west)
        {\textbf{\color{magenta5} 性质~#2}};
        % \draw[color=magenta5,line width=2pt] ([xshift=3.5pt,yshift=-5.5mm] frame.north west)--([yshift=-5.5mm] frame.north east);
    }, %overlay
    % 保持边缘的变化
    overlay last    = {
        %垂直线段%跨页时生效
        \draw[color=magenta5,  line width=2pt] ([xshift=2pt] frame.north west)--([xshift=2pt] frame.south west);
    }
    #1
}
%
\NewDocumentEnvironment{conclusion}{O{} O{} O{}}{\smallskip\begin{keypointbox}{#1}{#2} #3}{\end{keypointbox}}

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\RequirePackage{mdframed}
\RequirePackage{varwidth,multicol}
\newcommand{\eqskip}{\hspace{2.0em}}
\newcommand{\titem}{\hspace{0.5em} - }
\newcommand{\exitem}{\hspace{1.0em} - }
\newcommand{\yskip}{\vspace{0.6em}}
\newcommand{\bxskip}{\vspace{-1.5em}}
\newcommand{\lskip}{\hspace{2.0em}}
\newcommand{\rcyskip}{\vspace*{-4pt}} 
\newcommand{\eqyskip}{\vspace*{0.5em}}  
\newcommand{\xskip}{}
\newcommand{\oskip}{}
\newcommand{\ra}{\, \Rightarrow \,}
\newcommand{\RA}{\quad \Rightarrow \quad}
\newcommand{\ee}{\mathrm{e}}
\newcommand{\opdt}{\frac{\mathrm{d}}{\mathrm{d}t}}
\newcommand{\ddt}[1]{\frac{\mathrm{d}#1}{\mathrm{d}t}}
\newcommand{\dddt}[1]{\frac{\mathrm{d}^2#1}{\mathrm{d}t^2}}
\newcommand{\avg}[1]{\left< #1 \right>}
\newcommand{\tspace}{\text{ }}
\newcommand{\sunit}[1]{\text{ #1}}
\newcommand{\tmax}{\text{max}}
\newcommand{\tmin}{\text{min}}
\newcommand{\rad}{\text{ rad}}
\newcommand{\radps}{\text{ rad s$^{-1}$}}
\newcommand{\jpkgk}{\text{ J kg$^{-1}$ K$^{-1}$}}
\newcommand{\jpkgC}{\text{ J kg$^{-1}$ $^\circ\text{C}^{-1}$}}
\newcommand{\mps}{\text{ m s$^{-1}$}}
\newcommand{\cmps}{\text{ cm s$^{-1}$}}
\newcommand{\mpss}{\text{ m s$^{-2}$}}
\newcommand{\kgmpss}{\text{ kg m s$^{-2}$}}
\newcommand{\Vpm}{\text{ V m$^{-1}$}}
\newcommand{\Vpcm}{\text{ V cm$^{-1}$}}
\newcommand{\NpC}{\text{ N C$^{-1}$}}
\newcommand{\muF}{\text{ $\mu$F}}
\newcommand{\pF}{\text{ pF}}
\newcommand{\OC}{\text{$^\circ\text{C}$}}
\newcommand{\halflife}{t_\text{\tiny\sfrac{1}{2}}}
\newcommand{\fnet}{F_\text{net}}
\newcommand{\ec}{4\pi\epsilon_0}
\newcommand{\Mp}{m_\text{p}}
\newcommand{\Mn}{m_\text{n}}
\newcommand{\eoe}{\hfill $\square$}
\newcommand{\teoe}{\tag*{$\square$}}
\newcommand{\feoe}{\vspace*{-\baselineskip}\hfill $\square$}
\newcommand{\up}{\nnearrow}
\newcommand{\down}{\ssearrow}

\definecolor{DeepBlue}{RGB}{43,59,83}
\definecolor{Deepblue}{RGB}{75,101,134}
\definecolor{Blue}{RGB}{113,151,172}
\definecolor{Wine}{RGB}{136,63,75}
\definecolor{Pink}{RGB}{226,209,211}
\definecolor{Deepviolet}{RGB}{77,49,78}
\definecolor{Violet}{RGB}{103,76,120}
\DeclareTColorBox{ascolorbox}{ o m O{}}%
{enhanced,colback=white, skin=enhancedlast jigsaw,breakable, attach boxed title to top left={xshift=-4mm,yshift=-0.5mm}, fonttitle=\bfseries\rmfamily, varwidth boxed title=0.85\linewidth, colbacktitle=Violet,colframe=Violet,
    boxed title style={empty,arc=0pt,outer arc=0pt,boxrule=0pt},
    underlay boxed title={%
        \fill[Violet] (title.north west) -- (title.north east) -- +(\tcboxedtitleheight-1mm,-\tcboxedtitleheight+1mm)
        -- ([xshift=4mm,yshift=0.5mm]frame.north east) -- +(0mm,-1mm) -- (title.south west) -- cycle;
        \fill[Deepviolet] ([yshift=-0.5mm]frame.north west)
        -- +(-0.4,0) -- +(0,-0.3) -- cycle;
        \fill[Deepviolet] ([yshift=-0.5mm]frame.north east)
        -- +(0,-0.3) -- +(0.4,0) -- cycle;  },
    IfValueTF={#1}{title=#2〈#1〉}{title=#2},#3}

\newcommand{\cmt}{\noindent\hspace{-0.25em}\textcolor{cyan}{\ding{226}} \hspace{0.2em}}
\newcommand{\sol}{\noindent\hspace{-0.12em}\textcolor{cyan}{\ding{45}} \hspace{0.2em}}
\newcommand{\solc}{\noindent\hspace{-0.12em}\textcolor{cyan}{\ding{45}} \hspace{0.2em}} 

%%%%%%%%%%%%%%%%问题环境定制%%%%%%%%%%%%%%%%
\newcounter{question}[section]
\renewcommand{\thequestion}{\arabic{section}.\arabic{question}}
\newcommand{\questions}[1]{%
    \refstepcounter{question}
    \noindent{\textcolor{magenta}{\textsf{\textbf{Question \thequestion }}} \hspace*{1pt} #1 %
}}

%% 练习
\newcounter{reidaibangou}
\newtcolorbox{reidai}[1][]{breakable,enhanced,boxrule=0.5mm,
    top=2pt,left=44pt,right=4pt,bottom=2pt,arc=0mm,
    colframe=blue!30!gray,
    boxrule=1pt,
    underlay={
        \node[inner sep=1pt,blue!50!black,fill=blue!10!white]at ([xshift=22pt,yshift=-9pt]interior.north west) {\stepcounter{reidaibangou}\bfseries\rmfamily 练习 \thereidaibangou};},
    segmentation code={%
        \draw[dashed] (segmentation.west)--(segmentation.east);
        \node[inner sep=1pt,blue!50!black,fill=blue!10!white] at ([xshift=22pt,yshift=-8pt]segmentation.south west) {\bfseries\rmfamily 解};},
    before upper={\setlength{\parindent}{1mm}},
    before lower={\setlength{\parindent}{1mm}},#1
}

%%%%%%%%%%%%%%%编程语言显示定制%%%%%%%%%%%%%%%
\lstdefinestyle{c}{
  language=C,
  basicstyle=%
    \ttfamily
    \lst@ifdisplaystyle\small\fi,
  style=mystyle,
  morekeywords={asm,auto,double,int,struct,break,else,long,switch,case,enum,register,typedef,char,extern,return,union,const,float,short,unsigned,continue,for,signed,void,default,goto,sizeof,volatile,do,if,while,static,alignas,__attribute__,\#define,\#include,\#undef,\#ifdef,\#ifndef,\#if,\#else,\#elif,\#endif,\#error,\#pragma},
}

\lstdefinestyle{cpp}{
  language=C,
  basicstyle=%
    \ttfamily
    \lst@ifdisplaystyle\small\fi,
  style=mystyle,
  morekeywords={asm,auto,alignas,decltype,namespace,struct,alignof,default,new,switch,and,delete,noexcept,template,and_eq,do,not,this,asm,double,not_eq,thread_local,auto,dynamic_cast,nullptr,throw,bitand,else,operator,true,bitor,enum,or,try,bool,explicit,or_eq,typedef,break,export,private,typeid,case,extern,protected,typename,catch,false,public,union,char,float,register,unsigned,char16_t,for,reinterpret_cast,using,char32_t,friend,return,virtual,class,goto,short,void,compl,if,signed,volatile,const,inline,sizeof,wchar_t,constexpr,int,static,while,const_cast,long,static_assert,xor,continue,mutable,static_cast,xor_eq,__attribute__,\#define,\#include,\#undef,\#ifdef,\#ifndef,\#if,\#else,\#elif,\#endif,\#error,\#pragma},
}

\lstdefinestyle{c51}{
  language=C,
  basicstyle=%
    \ttfamily
    \lst@ifdisplaystyle\small\fi,
  style=mystyle,
  morekeywords={asm,auto,double,int,struct,break,else,long,switch,case,enum,register,typedef,char,extern,return,union,const,float,short,unsigned,continue,for,signed,void,default,goto,sizeof,volatile,do,if,while,static,sbit,sfr,_at_,alien,bdata,bit,code,compact,data,far,idata,interrupt,large,pdata,_priority_,__attribute__,reentrant,sbit,sfr,sfr16,small,_task_,using,xdata,edata,\#define,\#include,\#undef,\#ifdef,\#ifndef,\#if,\#else,\#elif,\#endif,\#error,\#pragma},
}

\lstdefinestyle{csharp}{
  language=C,
  basicstyle=%
    \ttfamily
    \lst@ifdisplaystyle\small\fi,
  style=mystyle,
  morekeywords={abstract,as,base,bool,break,byte,case,catch,char,checked,class,const,continue,decimal,default,delegate,do,double,else,enum,event,explicit,extern,false,finally,fixed,float,for,foreach,goto,if,implicit,in,int,interface,internal,is,lock,long,namespace,new,null,object,operator,out,override,params,private,protected,public,readonly,ref,return,sbyte,sealed,short,sizeof,stackalloc,static,string,struct,switch,this,throw,true,try,typeof,uint,ulong,unchecked,unsafe,ushort,using,virtual,void,volatile,add,and,alias,ascending,args,async,await,by,descending,dynamic,equals,from,get,global,group,init,into,join,let,nameof,nint,not,notnull,nuint,on,or,orderby,partial,record,remove,select,set,unmanaged,value,var,when,where,with,yield},
}

% 引用样式
\usepackage{cprotect}
\usepackage{array}
\usepackage{zhlipsum}
\usepackage{longtable}
\usepackage{animate}
\usepackage{setspace}
\usepackage{datetime}
\setcounter{tocdepth}{2}
\colorlet{coverlinecolor}{structurecolor}

% 定义bibitem的格式
\newcounter{bibitem-cnt}[chapter]
\newcommand{\bbitem}{\bibitem{ref-\thechapter-\arabic{bibitem-cnt}}\refstepcounter{bibitem-cnt}}

% 定义摘要(abstract)环境 
\newenvironment{abstract}[2]{\chapter*{{\LARGE {#1}}\\{#2}} \thispagestyle{empty}}{}
% 关键字/词
\newcommand{\keyword}[2]{\\ \hspace*{\fill} \\ \noindent{\textbf{#1 : } #2}}

% 加粗
\renewcommand{\textbf}[1]{#1}
% 表格x-x
\renewcommand{\thetable}{\IfAppendix{\Alph{chapter}-\arabic{table}}{\arabic{chapter}-\arabic{table}}}
% 行距
\linespread{1.5}
