#include "mimes.h"
#include <map>

std::map<std::string, std::string> mp;

std::string find_mime(const std::string &ext)
{
	if (!mp.count(ext))
		return "application/x-binary";
	return mp.at(ext);
}

void init_mimes()
{
	mp[".3dm"]="x-world/x-3dmf";
	mp[".3dmf"]="x-world/x-3dmf";
	mp[".a"]="application/octet-stream";
	mp[".aab"]="application/x-authorware-bin";
	mp[".aam"]="application/x-authorware-map";
	mp[".aas"]="application/x-authorware-seg";
	mp[".abc"]="text/vnd.abc";
	mp[".acgi"]="text/html";
	mp[".afl"]="video/animaflex";
	mp[".ai"]="application/postscript";
	mp[".aif"]="audio/aiff";
	mp[".aif"]="audio/x-aiff";
	mp[".aifc"]="audio/aiff";
	mp[".aifc"]="audio/x-aiff";
	mp[".aiff"]="audio/aiff";
	mp[".aiff"]="audio/x-aiff";
	mp[".aim"]="application/x-aim";
	mp[".aip"]="text/x-audiosoft-intra";
	mp[".ani"]="application/x-navi-animation";
	mp[".aos"]="application/x-nokia-9000-communicator-add-on-software";
	mp[".aps"]="application/mime";
	mp[".arc"]="application/octet-stream";
	mp[".arj"]="application/arj";
	mp[".arj"]="application/octet-stream";
	mp[".art"]="image/x-jg";
	mp[".asf"]="video/x-ms-asf";
	mp[".asm"]="text/x-asm";
	mp[".asp"]="text/asp";
	mp[".asx"]="application/x-mplayer2";
	mp[".asx"]="video/x-ms-asf";
	mp[".asx"]="video/x-ms-asf-plugin";
	mp[".au"]="audio/basic";
	mp[".au"]="audio/x-au";
	mp[".avi"]="application/x-troff-msvideo";
	mp[".avi"]="video/avi";
	mp[".avi"]="video/msvideo";
	mp[".avi"]="video/x-msvideo";
	mp[".avs"]="video/avs-video";
	mp[".bcpio"]="application/x-bcpio";
	mp[".bin"]="application/mac-binary";
	mp[".bin"]="application/macbinary";
	mp[".bin"]="application/octet-stream";
	mp[".bin"]="application/x-binary";
	mp[".bin"]="application/x-macbinary";
	mp[".bm"]="image/bmp";
	mp[".bmp"]="image/bmp";
	mp[".bmp"]="image/x-windows-bmp";
	mp[".boo"]="application/book";
	mp[".book"]="application/book";
	mp[".boz"]="application/x-bzip2";
	mp[".bsh"]="application/x-bsh";
	mp[".bz"]="application/x-bzip";
	mp[".bz2"]="application/x-bzip2";
	mp[".c"]="text/plain";
	mp[".c"]="text/x-c";
	mp[".c++"]="text/plain";
	mp[".cat"]="application/vnd.ms-pki.seccat";
	mp[".cc"]="text/plain";
	mp[".cc"]="text/x-c";
	mp[".ccad"]="application/clariscad";
	mp[".cco"]="application/x-cocoa";
	mp[".cdf"]="application/cdf";
	mp[".cdf"]="application/x-cdf";
	mp[".cdf"]="application/x-netcdf";
	mp[".cer"]="application/pkix-cert";
	mp[".cer"]="application/x-x509-ca-cert";
	mp[".cha"]="application/x-chat";
	mp[".chat"]="application/x-chat";
	mp[".class"]="application/java";
	mp[".class"]="application/java-byte-code";
	mp[".class"]="application/x-java-class";
	mp[".com"]="application/octet-stream";
	mp[".com"]="text/plain";
	mp[".conf"]="text/plain";
	mp[".cpio"]="application/x-cpio";
	mp[".cpp"]="text/x-c";
	mp[".cpt"]="application/mac-compactpro";
	mp[".cpt"]="application/x-compactpro";
	mp[".cpt"]="application/x-cpt";
	mp[".crl"]="application/pkcs-crl";
	mp[".crl"]="application/pkix-crl";
	mp[".crt"]="application/pkix-cert";
	mp[".crt"]="application/x-x509-ca-cert";
	mp[".crt"]="application/x-x509-user-cert";
	mp[".csh"]="application/x-csh";
	mp[".csh"]="text/x-script.csh";
	mp[".css"]="application/x-pointplus";
	mp[".css"]="text/css";
	mp[".cxx"]="text/plain";
	mp[".dcr"]="application/x-director";
	mp[".deepv"]="application/x-deepv";
	mp[".def"]="text/plain";
	mp[".der"]="application/x-x509-ca-cert";
	mp[".dif"]="video/x-dv";
	mp[".dir"]="application/x-director";
	mp[".dl"]="video/dl";
	mp[".dl"]="video/x-dl";
	mp[".doc"]="application/msword";
	mp[".dot"]="application/msword";
	mp[".dp"]="application/commonground";
	mp[".drw"]="application/drafting";
	mp[".dump"]="application/octet-stream";
	mp[".dv"]="video/x-dv";
	mp[".dvi"]="application/x-dvi";
	mp[".dwf"]="drawing/x-dwf (old)";
	mp[".dwf"]="model/vnd.dwf";
	mp[".dwg"]="application/acad";
	mp[".dwg"]="image/vnd.dwg";
	mp[".dwg"]="image/x-dwg";
	mp[".dxf"]="application/dxf";
	mp[".dxf"]="image/vnd.dwg";
	mp[".dxf"]="image/x-dwg";
	mp[".dxr"]="application/x-director";
	mp[".el"]="text/x-script.elisp";
	mp[".elc"]="application/x-bytecode.elisp (compiled elisp)";
	mp[".elc"]="application/x-elc";
	mp[".env"]="application/x-envoy";
	mp[".eps"]="application/postscript";
	mp[".es"]="application/x-esrehber";
	mp[".etx"]="text/x-setext";
	mp[".evy"]="application/envoy";
	mp[".evy"]="application/x-envoy";
	mp[".exe"]="application/octet-stream";
	mp[".f"]="text/plain";
	mp[".f"]="text/x-fortran";
	mp[".f77"]="text/x-fortran";
	mp[".f90"]="text/plain";
	mp[".f90"]="text/x-fortran";
	mp[".fdf"]="application/vnd.fdf";
	mp[".fif"]="application/fractals";
	mp[".fif"]="image/fif";
	mp[".fli"]="video/fli";
	mp[".fli"]="video/x-fli";
	mp[".flo"]="image/florian";
	mp[".flx"]="text/vnd.fmi.flexstor";
	mp[".fmf"]="video/x-atomic3d-feature";
	mp[".for"]="text/plain";
	mp[".for"]="text/x-fortran";
	mp[".fpx"]="image/vnd.fpx";
	mp[".fpx"]="image/vnd.net-fpx";
	mp[".frl"]="application/freeloader";
	mp[".funk"]="audio/make";
	mp[".g"]="text/plain";
	mp[".g3"]="image/g3fax";
	mp[".gif"]="image/gif";
	mp[".gl"]="video/gl";
	mp[".gl"]="video/x-gl";
	mp[".gsd"]="audio/x-gsm";
	mp[".gsm"]="audio/x-gsm";
	mp[".gsp"]="application/x-gsp";
	mp[".gss"]="application/x-gss";
	mp[".gtar"]="application/x-gtar";
	mp[".gz"]="application/x-compressed";
	mp[".gz"]="application/x-gzip";
	mp[".gzip"]="application/x-gzip";
	mp[".gzip"]="multipart/x-gzip";
	mp[".h"]="text/plain";
	mp[".h"]="text/x-h";
	mp[".hdf"]="application/x-hdf";
	mp[".help"]="application/x-helpfile";
	mp[".hgl"]="application/vnd.hp-hpgl";
	mp[".hh"]="text/plain";
	mp[".hh"]="text/x-h";
	mp[".hlb"]="text/x-script";
	mp[".hlp"]="application/hlp";
	mp[".hlp"]="application/x-helpfile";
	mp[".hlp"]="application/x-winhelp";
	mp[".hpg"]="application/vnd.hp-hpgl";
	mp[".hpgl"]="application/vnd.hp-hpgl";
	mp[".hqx"]="application/binhex";
	mp[".hqx"]="application/binhex4";
	mp[".hqx"]="application/mac-binhex";
	mp[".hqx"]="application/mac-binhex40";
	mp[".hqx"]="application/x-binhex40";
	mp[".hqx"]="application/x-mac-binhex40";
	mp[".hta"]="application/hta";
	mp[".htc"]="text/x-component";
	mp[".htm"]="text/html";
	mp[".html"]="text/html";
	mp[".htmls"]="text/html";
	mp[".htt"]="text/webviewhtml";
	mp[".htx "]="text/html";
	mp[".ice "]="x-conference/x-cooltalk";
	mp[".ico"]="image/x-icon";
	mp[".idc"]="text/plain";
	mp[".ief"]="image/ief";
	mp[".iefs"]="image/ief";
	mp[".iges"]="application/iges";
	mp[".iges "]="model/iges";
	mp[".igs"]="application/iges";
	mp[".igs"]="model/iges";
	mp[".ima"]="application/x-ima";
	mp[".imap"]="application/x-httpd-imap";
	mp[".inf "]="application/inf";
	mp[".ins"]="application/x-internett-signup";
	mp[".ip "]="application/x-ip2";
	mp[".isu"]="video/x-isvideo";
	mp[".it"]="audio/it";
	mp[".iv"]="application/x-inventor";
	mp[".ivr"]="i-world/i-vrml";
	mp[".ivy"]="application/x-livescreen";
	mp[".jam "]="audio/x-jam";
	mp[".jav"]="text/plain";
	mp[".jav"]="text/x-java-source";
	mp[".java"]="text/plain";
	mp[".java "]="text/x-java-source";
	mp[".jcm "]="application/x-java-commerce";
	mp[".jfif"]="image/jpeg";
	mp[".jfif"]="image/pjpeg";
	mp[".jfif-tbnl"]="image/jpeg";
	mp[".jpe"]="image/jpeg";
	mp[".jpe"]="image/pjpeg";
	mp[".jpeg"]="image/jpeg";
	mp[".jpeg"]="image/pjpeg";
	mp[".jpg "]="image/jpeg";
	mp[".jpg "]="image/pjpeg";
	mp[".jps"]="image/x-jps";
	mp[".js "]="application/x-javascript";
	mp[".jut"]="image/jutvision";
	mp[".kar"]="audio/midi";
	mp[".kar"]="music/x-karaoke";
	mp[".ksh"]="application/x-ksh";
	mp[".ksh"]="text/x-script.ksh";
	mp[".la "]="audio/nspaudio";
	mp[".la "]="audio/x-nspaudio";
	mp[".lam"]="audio/x-liveaudio";
	mp[".latex "]="application/x-latex";
	mp[".lha"]="application/lha";
	mp[".lha"]="application/octet-stream";
	mp[".lha"]="application/x-lha";
	mp[".lhx"]="application/octet-stream";
	mp[".list"]="text/plain";
	mp[".lma"]="audio/nspaudio";
	mp[".lma"]="audio/x-nspaudio";
	mp[".log "]="text/plain";
	mp[".lsp "]="application/x-lisp";
	mp[".lsp "]="text/x-script.lisp";
	mp[".lst "]="text/plain";
	mp[".lsx"]="text/x-la-asf";
	mp[".ltx"]="application/x-latex";
	mp[".lzh"]="application/octet-stream";
	mp[".lzh"]="application/x-lzh";
	mp[".lzx"]="application/lzx";
	mp[".lzx"]="application/octet-stream";
	mp[".lzx"]="application/x-lzx";
	mp[".m"]="text/plain";
	mp[".m"]="text/x-m";
	mp[".m1v"]="video/mpeg";
	mp[".m2a"]="audio/mpeg";
	mp[".m2v"]="video/mpeg";
	mp[".m3u "]="audio/x-mpequrl";
	mp[".man"]="application/x-troff-man";
	mp[".map"]="application/x-navimap";
	mp[".mar"]="text/plain";
	mp[".mbd"]="application/mbedlet";
	mp[".mc$"]="application/x-magic-cap-package-1.0";
	mp[".mcd"]="application/mcad";
	mp[".mcd"]="application/x-mathcad";
	mp[".mcf"]="image/vasa";
	mp[".mcf"]="text/mcf";
	mp[".mcp"]="application/netmc";
	mp[".me "]="application/x-troff-me";
	mp[".mht"]="message/rfc822";
	mp[".mhtml"]="message/rfc822";
	mp[".mid"]="application/x-midi";
	mp[".mid"]="audio/midi";
	mp[".mid"]="audio/x-mid";
	mp[".mid"]="audio/x-midi";
	mp[".mid"]="music/crescendo";
	mp[".mid"]="x-music/x-midi";
	mp[".midi"]="application/x-midi";
	mp[".midi"]="audio/midi";
	mp[".midi"]="audio/x-mid";
	mp[".midi"]="audio/x-midi";
	mp[".midi"]="music/crescendo";
	mp[".midi"]="x-music/x-midi";
	mp[".mif"]="application/x-frame";
	mp[".mif"]="application/x-mif";
	mp[".mime "]="message/rfc822";
	mp[".mime "]="www/mime";
	mp[".mjf"]="audio/x-vnd.audioexplosion.mjuicemediafile";
	mp[".mjpg "]="video/x-motion-jpeg";
	mp[".mm"]="application/base64";
	mp[".mm"]="application/x-meme";
	mp[".mme"]="application/base64";
	mp[".mod"]="audio/mod";
	mp[".mod"]="audio/x-mod";
	mp[".moov"]="video/quicktime";
	mp[".mov"]="video/quicktime";
	mp[".movie"]="video/x-sgi-movie";
	mp[".mp2"]="audio/mpeg";
	mp[".mp2"]="audio/x-mpeg";
	mp[".mp2"]="video/mpeg";
	mp[".mp2"]="video/x-mpeg";
	mp[".mp2"]="video/x-mpeq2a";
	mp[".mp3"]="audio/mpeg3";
	mp[".mp3"]="audio/x-mpeg-3";
	mp[".mp3"]="video/mpeg";
	mp[".mp3"]="video/x-mpeg";
	mp[".mpa"]="audio/mpeg";
	mp[".mpa"]="video/mpeg";
	mp[".mpc"]="application/x-project";
	mp[".mpe"]="video/mpeg";
	mp[".mpeg"]="video/mpeg";
	mp[".mpg"]="audio/mpeg";
	mp[".mpg"]="video/mpeg";
	mp[".mpga"]="audio/mpeg";
	mp[".mpp"]="application/vnd.ms-project";
	mp[".mpt"]="application/x-project";
	mp[".mpv"]="application/x-project";
	mp[".mpx"]="application/x-project";
	mp[".mrc"]="application/marc";
	mp[".ms"]="application/x-troff-ms";
	mp[".mv"]="video/x-sgi-movie";
	mp[".my"]="audio/make";
	mp[".mzz"]="application/x-vnd.audioexplosion.mzz";
	mp[".nap"]="image/naplps";
	mp[".naplps"]="image/naplps";
	mp[".nc"]="application/x-netcdf";
	mp[".ncm"]="application/vnd.nokia.configuration-message";
	mp[".nif"]="image/x-niff";
	mp[".niff"]="image/x-niff";
	mp[".nix"]="application/x-mix-transfer";
	mp[".nsc"]="application/x-conference";
	mp[".nvd"]="application/x-navidoc";
	mp[".o"]="application/octet-stream";
	mp[".oda"]="application/oda";
	mp[".omc"]="application/x-omc";
	mp[".omcd"]="application/x-omcdatamaker";
	mp[".omcr"]="application/x-omcregerator";
	mp[".p"]="text/x-pascal";
	mp[".p10"]="application/pkcs10";
	mp[".p10"]="application/x-pkcs10";
	mp[".p12"]="application/pkcs-12";
	mp[".p12"]="application/x-pkcs12";
	mp[".p7a"]="application/x-pkcs7-signature";
	mp[".p7c"]="application/pkcs7-mime";
	mp[".p7c"]="application/x-pkcs7-mime";
	mp[".p7m"]="application/pkcs7-mime";
	mp[".p7m"]="application/x-pkcs7-mime";
	mp[".p7r"]="application/x-pkcs7-certreqresp";
	mp[".p7s"]="application/pkcs7-signature";
	mp[".part "]="application/pro_eng";
	mp[".pas"]="text/pascal";
	mp[".pbm "]="image/x-portable-bitmap";
	mp[".pcl"]="application/vnd.hp-pcl";
	mp[".pcl"]="application/x-pcl";
	mp[".pct"]="image/x-pict";
	mp[".pcx"]="image/x-pcx";
	mp[".pdb"]="chemical/x-pdb";
	mp[".pdf"]="application/pdf";
	mp[".pfunk"]="audio/make";
	mp[".pfunk"]="audio/make.my.funk";
	mp[".pgm"]="image/x-portable-graymap";
	mp[".pgm"]="image/x-portable-greymap";
	mp[".pic"]="image/pict";
	mp[".pict"]="image/pict";
	mp[".pkg"]="application/x-newton-compatible-pkg";
	mp[".pko"]="application/vnd.ms-pki.pko";
	mp[".pl"]="text/plain";
	mp[".pl"]="text/x-script.perl";
	mp[".plx"]="application/x-pixclscript";
	mp[".pm"]="image/x-xpixmap";
	mp[".pm"]="text/x-script.perl-module";
	mp[".pm4 "]="application/x-pagemaker";
	mp[".pm5"]="application/x-pagemaker";
	mp[".png"]="image/png";
	mp[".pnm"]="application/x-portable-anymap";
	mp[".pnm"]="image/x-portable-anymap";
	mp[".pot"]="application/mspowerpoint";
	mp[".pot"]="application/vnd.ms-powerpoint";
	mp[".pov"]="model/x-pov";
	mp[".ppa"]="application/vnd.ms-powerpoint";
	mp[".ppm"]="image/x-portable-pixmap";
	mp[".pps"]="application/mspowerpoint";
	mp[".pps"]="application/vnd.ms-powerpoint";
	mp[".ppt"]="application/mspowerpoint";
	mp[".ppt"]="application/powerpoint";
	mp[".ppt"]="application/vnd.ms-powerpoint";
	mp[".ppt"]="application/x-mspowerpoint";
	mp[".ppz"]="application/mspowerpoint";
	mp[".pre"]="application/x-freelance";
	mp[".prt"]="application/pro_eng";
	mp[".ps"]="application/postscript";
	mp[".psd"]="application/octet-stream";
	mp[".pvu"]="paleovu/x-pv";
	mp[".pwz "]="application/vnd.ms-powerpoint";
	mp[".py "]="text/x-script.phyton";
	mp[".pyc "]="applicaiton/x-bytecode.python";
	mp[".qcp "]="audio/vnd.qcelp";
	mp[".qd3 "]="x-world/x-3dmf";
	mp[".qd3d "]="x-world/x-3dmf";
	mp[".qif"]="image/x-quicktime";
	mp[".qt"]="video/quicktime";
	mp[".qtc"]="video/x-qtc";
	mp[".qti"]="image/x-quicktime";
	mp[".qtif"]="image/x-quicktime";
	mp[".ra"]="audio/x-pn-realaudio";
	mp[".ra"]="audio/x-pn-realaudio-plugin";
	mp[".ra"]="audio/x-realaudio";
	mp[".ram"]="audio/x-pn-realaudio";
	mp[".ras"]="application/x-cmu-raster";
	mp[".ras"]="image/cmu-raster";
	mp[".ras"]="image/x-cmu-raster";
	mp[".rast"]="image/cmu-raster";
	mp[".rexx "]="text/x-script.rexx";
	mp[".rf"]="image/vnd.rn-realflash";
	mp[".rgb "]="image/x-rgb";
	mp[".rm"]="application/vnd.rn-realmedia";
	mp[".rm"]="audio/x-pn-realaudio";
	mp[".rmi"]="audio/mid";
	mp[".rmm "]="audio/x-pn-realaudio";
	mp[".rmp"]="audio/x-pn-realaudio";
	mp[".rmp"]="audio/x-pn-realaudio-plugin";
	mp[".rng"]="application/ringing-tones";
	mp[".rng"]="application/vnd.nokia.ringing-tone";
	mp[".rnx "]="application/vnd.rn-realplayer";
	mp[".roff"]="application/x-troff";
	mp[".rp "]="image/vnd.rn-realpix";
	mp[".rpm"]="audio/x-pn-realaudio-plugin";
	mp[".rt"]="text/richtext";
	mp[".rt"]="text/vnd.rn-realtext";
	mp[".rtf"]="application/rtf";
	mp[".rtf"]="application/x-rtf";
	mp[".rtf"]="text/richtext";
	mp[".rtx"]="application/rtf";
	mp[".rtx"]="text/richtext";
	mp[".rv"]="video/vnd.rn-realvideo";
	mp[".s"]="text/x-asm";
	mp[".s3m "]="audio/s3m";
	mp[".saveme"]="application/octet-stream";
	mp[".sbk "]="application/x-tbook";
	mp[".scm"]="application/x-lotusscreencam";
	mp[".scm"]="text/x-script.guile";
	mp[".scm"]="text/x-script.scheme";
	mp[".scm"]="video/x-scm";
	mp[".sdml"]="text/plain";
	mp[".sdp "]="application/sdp";
	mp[".sdp "]="application/x-sdp";
	mp[".sdr"]="application/sounder";
	mp[".sea"]="application/sea";
	mp[".sea"]="application/x-sea";
	mp[".set"]="application/set";
	mp[".sgm "]="text/sgml";
	mp[".sgm "]="text/x-sgml";
	mp[".sgml"]="text/sgml";
	mp[".sgml"]="text/x-sgml";
	mp[".sh"]="application/x-bsh";
	mp[".sh"]="application/x-sh";
	mp[".sh"]="application/x-shar";
	mp[".sh"]="text/x-script.sh";
	mp[".shar"]="application/x-bsh";
	mp[".shar"]="application/x-shar";
	mp[".shtml "]="text/html";
	mp[".shtml"]="text/x-server-parsed-html";
	mp[".sid"]="audio/x-psid";
	mp[".sit"]="application/x-sit";
	mp[".sit"]="application/x-stuffit";
	mp[".skd"]="application/x-koan";
	mp[".skm "]="application/x-koan";
	mp[".skp "]="application/x-koan";
	mp[".skt "]="application/x-koan";
	mp[".sl "]="application/x-seelogo";
	mp[".smi "]="application/smil";
	mp[".smil "]="application/smil";
	mp[".snd"]="audio/basic";
	mp[".snd"]="audio/x-adpcm";
	mp[".sol"]="application/solids";
	mp[".spc "]="application/x-pkcs7-certificates";
	mp[".spc "]="text/x-speech";
	mp[".spl"]="application/futuresplash";
	mp[".spr"]="application/x-sprite";
	mp[".sprite "]="application/x-sprite";
	mp[".src"]="application/x-wais-source";
	mp[".ssi"]="text/x-server-parsed-html";
	mp[".ssm "]="application/streamingmedia";
	mp[".sst"]="application/vnd.ms-pki.certstore";
	mp[".step"]="application/step";
	mp[".stl"]="application/sla";
	mp[".stl"]="application/vnd.ms-pki.stl";
	mp[".stl"]="application/x-navistyle";
	mp[".stp"]="application/step";
	mp[".sv4cpio"]="application/x-sv4cpio";
	mp[".sv4crc"]="application/x-sv4crc";
	mp[".svf"]="image/vnd.dwg";
	mp[".svf"]="image/x-dwg";
	mp[".svr"]="application/x-world";
	mp[".svr"]="x-world/x-svr";
	mp[".swf"]="application/x-shockwave-flash";
	mp[".t"]="application/x-troff";
	mp[".talk"]="text/x-speech";
	mp[".tar"]="application/x-tar";
	mp[".tbk"]="application/toolbook";
	mp[".tbk"]="application/x-tbook";
	mp[".tcl"]="application/x-tcl";
	mp[".tcl"]="text/x-script.tcl";
	mp[".tcsh"]="text/x-script.tcsh";
	mp[".tex"]="application/x-tex";
	mp[".texi"]="application/x-texinfo";
	mp[".texinfo"]="application/x-texinfo";
	mp[".text"]="application/plain";
	mp[".text"]="text/plain";
	mp[".tgz"]="application/gnutar";
	mp[".tgz"]="application/x-compressed";
	mp[".tif"]="image/tiff";
	mp[".tif"]="image/x-tiff";
	mp[".tiff"]="image/tiff";
	mp[".tiff"]="image/x-tiff";
	mp[".tr"]="application/x-troff";
	mp[".tsi"]="audio/tsp-audio";
	mp[".tsp"]="application/dsptype";
	mp[".tsp"]="audio/tsplayer";
	mp[".tsv"]="text/tab-separated-values";
	mp[".turbot"]="image/florian";
	mp[".txt"]="text/plain";
	mp[".uil"]="text/x-uil";
	mp[".uni"]="text/uri-list";
	mp[".unis"]="text/uri-list";
	mp[".unv"]="application/i-deas";
	mp[".uri"]="text/uri-list";
	mp[".uris"]="text/uri-list";
	mp[".ustar"]="application/x-ustar";
	mp[".ustar"]="multipart/x-ustar";
	mp[".uu"]="application/octet-stream";
	mp[".uu"]="text/x-uuencode";
	mp[".uue"]="text/x-uuencode";
	mp[".vcd"]="application/x-cdlink";
	mp[".vcs"]="text/x-vcalendar";
	mp[".vda"]="application/vda";
	mp[".vdo"]="video/vdo";
	mp[".vew "]="application/groupwise";
	mp[".viv"]="video/vivo";
	mp[".viv"]="video/vnd.vivo";
	mp[".vivo"]="video/vivo";
	mp[".vivo"]="video/vnd.vivo";
	mp[".vmd "]="application/vocaltec-media-desc";
	mp[".vmf"]="application/vocaltec-media-file";
	mp[".voc"]="audio/voc";
	mp[".voc"]="audio/x-voc";
	mp[".vos"]="video/vosaic";
	mp[".vox"]="audio/voxware";
	mp[".vqe"]="audio/x-twinvq-plugin";
	mp[".vqf"]="audio/x-twinvq";
	mp[".vql"]="audio/x-twinvq-plugin";
	mp[".vrml"]="application/x-vrml";
	mp[".vrml"]="model/vrml";
	mp[".vrml"]="x-world/x-vrml";
	mp[".vrt"]="x-world/x-vrt";
	mp[".vsd"]="application/x-visio";
	mp[".vst"]="application/x-visio";
	mp[".vsw "]="application/x-visio";
	mp[".w60"]="application/wordperfect6.0";
	mp[".w61"]="application/wordperfect6.1";
	mp[".w6w"]="application/msword";
	mp[".wav"]="audio/wav";
	mp[".wav"]="audio/x-wav";
	mp[".wb1"]="application/x-qpro";
	mp[".wbmp"]="image/vnd.wap.wbmp";
	mp[".web"]="application/vnd.xara";
	mp[".wiz"]="application/msword";
	mp[".wk1"]="application/x-123";
	mp[".wmf"]="windows/metafile";
	mp[".wml"]="text/vnd.wap.wml";
	mp[".wmlc "]="application/vnd.wap.wmlc";
	mp[".wmls"]="text/vnd.wap.wmlscript";
	mp[".wmlsc "]="application/vnd.wap.wmlscriptc";
	mp[".word "]="application/msword";
	mp[".wp"]="application/wordperfect";
	mp[".wp5"]="application/wordperfect";
	mp[".wp5"]="application/wordperfect6.0";
	mp[".wp6 "]="application/wordperfect";
	mp[".wpd"]="application/wordperfect";
	mp[".wpd"]="application/x-wpwin";
	mp[".wq1"]="application/x-lotus";
	mp[".wri"]="application/mswrite";
	mp[".wri"]="application/x-wri";
	mp[".wrl"]="application/x-world";
	mp[".wrl"]="model/vrml";
	mp[".wrl"]="x-world/x-vrml";
	mp[".wrz"]="model/vrml";
	mp[".wrz"]="x-world/x-vrml";
	mp[".wsc"]="text/scriplet";
	mp[".wsrc"]="application/x-wais-source";
	mp[".wtk "]="application/x-wintalk";
	mp[".xbm"]="image/x-xbitmap";
	mp[".xbm"]="image/x-xbm";
	mp[".xbm"]="image/xbm";
	mp[".xdr"]="video/x-amt-demorun";
	mp[".xgz"]="xgl/drawing";
	mp[".xif"]="image/vnd.xiff";
	mp[".xl"]="application/excel";
	mp[".xla"]="application/excel";
	mp[".xla"]="application/x-excel";
	mp[".xla"]="application/x-msexcel";
	mp[".xlb"]="application/excel";
	mp[".xlb"]="application/vnd.ms-excel";
	mp[".xlb"]="application/x-excel";
	mp[".xlc"]="application/excel";
	mp[".xlc"]="application/vnd.ms-excel";
	mp[".xlc"]="application/x-excel";
	mp[".xld "]="application/excel";
	mp[".xld "]="application/x-excel";
	mp[".xlk"]="application/excel";
	mp[".xlk"]="application/x-excel";
	mp[".xll"]="application/excel";
	mp[".xll"]="application/vnd.ms-excel";
	mp[".xll"]="application/x-excel";
	mp[".xlm"]="application/excel";
	mp[".xlm"]="application/vnd.ms-excel";
	mp[".xlm"]="application/x-excel";
	mp[".xls"]="application/excel";
	mp[".xls"]="application/vnd.ms-excel";
	mp[".xls"]="application/x-excel";
	mp[".xls"]="application/x-msexcel";
	mp[".xlt"]="application/excel";
	mp[".xlt"]="application/x-excel";
	mp[".xlv"]="application/excel";
	mp[".xlv"]="application/x-excel";
	mp[".xlw"]="application/excel";
	mp[".xlw"]="application/vnd.ms-excel";
	mp[".xlw"]="application/x-excel";
	mp[".xlw"]="application/x-msexcel";
	mp[".xm"]="audio/xm";
	mp[".xml"]="application/xml";
	mp[".xml"]="text/xml";
	mp[".xmz"]="xgl/movie";
	mp[".xpix"]="application/x-vnd.ls-xpix";
	mp[".xpm"]="image/x-xpixmap";
	mp[".xpm"]="image/xpm";
	mp[".x-png"]="image/png";
	mp[".xsr"]="video/x-amt-showrun";
	mp[".xwd"]="image/x-xwd";
	mp[".xwd"]="image/x-xwindowdump";
	mp[".xyz"]="chemical/x-pdb";
	mp[".z"]="application/x-compress";
	mp[".z"]="application/x-compressed";
	mp[".zip"]="application/x-compressed";
	mp[".zip"]="application/x-zip-compressed";
	mp[".zip"]="application/zip";
	mp[".zip"]="multipart/x-zip";
	mp[".zoo"]="application/octet-stream";
	mp[".zsh"]="text/x-script.zsh";
}