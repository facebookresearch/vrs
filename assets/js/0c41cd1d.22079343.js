"use strict";(self.webpackChunkwebsite=self.webpackChunkwebsite||[]).push([[787],{15680:(e,t,a)=>{a.r(t),a.d(t,{MDXContext:()=>d,MDXProvider:()=>u,mdx:()=>y,useMDXComponents:()=>m,withMDXComponents:()=>p});var o=a(96540);function n(e,t,a){return t in e?Object.defineProperty(e,t,{value:a,enumerable:!0,configurable:!0,writable:!0}):e[t]=a,e}function r(){return r=Object.assign||function(e){for(var t=1;t<arguments.length;t++){var a=arguments[t];for(var o in a)Object.prototype.hasOwnProperty.call(a,o)&&(e[o]=a[o])}return e},r.apply(this,arguments)}function s(e,t){var a=Object.keys(e);if(Object.getOwnPropertySymbols){var o=Object.getOwnPropertySymbols(e);t&&(o=o.filter((function(t){return Object.getOwnPropertyDescriptor(e,t).enumerable}))),a.push.apply(a,o)}return a}function i(e){for(var t=1;t<arguments.length;t++){var a=null!=arguments[t]?arguments[t]:{};t%2?s(Object(a),!0).forEach((function(t){n(e,t,a[t])})):Object.getOwnPropertyDescriptors?Object.defineProperties(e,Object.getOwnPropertyDescriptors(a)):s(Object(a)).forEach((function(t){Object.defineProperty(e,t,Object.getOwnPropertyDescriptor(a,t))}))}return e}function l(e,t){if(null==e)return{};var a,o,n=function(e,t){if(null==e)return{};var a,o,n={},r=Object.keys(e);for(o=0;o<r.length;o++)a=r[o],t.indexOf(a)>=0||(n[a]=e[a]);return n}(e,t);if(Object.getOwnPropertySymbols){var r=Object.getOwnPropertySymbols(e);for(o=0;o<r.length;o++)a=r[o],t.indexOf(a)>=0||Object.prototype.propertyIsEnumerable.call(e,a)&&(n[a]=e[a])}return n}var d=o.createContext({}),p=function(e){return function(t){var a=m(t.components);return o.createElement(e,r({},t,{components:a}))}},m=function(e){var t=o.useContext(d),a=t;return e&&(a="function"==typeof e?e(t):i(i({},t),e)),a},u=function(e){var t=m(e.components);return o.createElement(d.Provider,{value:t},e.children)},c="mdxType",h={inlineCode:"code",wrapper:function(e){var t=e.children;return o.createElement(o.Fragment,{},t)}},f=o.forwardRef((function(e,t){var a=e.components,n=e.mdxType,r=e.originalType,s=e.parentName,d=l(e,["components","mdxType","originalType","parentName"]),p=m(a),u=n,c=p["".concat(s,".").concat(u)]||p[u]||h[u]||r;return a?o.createElement(c,i(i({ref:t},d),{},{components:a})):o.createElement(c,i({ref:t},d))}));function y(e,t){var a=arguments,n=t&&t.mdxType;if("string"==typeof e||n){var r=a.length,s=new Array(r);s[0]=f;var i={};for(var l in t)hasOwnProperty.call(t,l)&&(i[l]=t[l]);i.originalType=e,i[c]="string"==typeof e?e:n,s[1]=i;for(var d=2;d<r;d++)s[d]=a[d];return o.createElement.apply(null,s)}return o.createElement.apply(null,a)}f.displayName="MDXCreateElement"},33110:(e,t,a)=>{a.r(t),a.d(t,{assets:()=>l,contentTitle:()=>s,default:()=>u,frontMatter:()=>r,metadata:()=>i,toc:()=>d});var o=a(58168),n=(a(96540),a(15680));a(75489);const r={sidebar_position:10,title:"The vrsplayer App"},s=void 0,i={unversionedId:"vrsplayer",id:"vrsplayer",title:"The vrsplayer App",description:'The vrsplayer app lets you "play" a VRS like a multi-stream video, with audio if an audio stream is present.',source:"@site/docs/vrsplayer.md",sourceDirName:".",slug:"/vrsplayer",permalink:"/vrs/docs/vrsplayer",draft:!1,editUrl:"https://github.com/facebookresearch/vrs/edit/main/website/docs/vrsplayer.md",tags:[],version:"current",sidebarPosition:10,frontMatter:{sidebar_position:10,title:"The vrsplayer App"},sidebar:"tutorialSidebar",previous:{title:"The vrs Command Line Tool",permalink:"/vrs/docs/VrsCliTool"}},l={},d=[{value:"Playback Controls",id:"playback-controls",level:2},{value:"Overlay Selection",id:"overlay-selection",level:2},{value:"Tooltips",id:"tooltips",level:2},{value:"Menu Bar Commands",id:"menu-bar-commands",level:2},{value:"Layout",id:"layout",level:3},{value:"Audio",id:"audio",level:3},{value:"Presets",id:"presets",level:3},{value:"Context Menu",id:"context-menu",level:2},{value:"Playback &quot;fps&quot; Display",id:"playback-fps-display",level:2},{value:"Keyboard Playback Controls",id:"keyboard-playback-controls",level:2}],p={toc:d},m="wrapper";function u(e){let{components:t,...r}=e;return(0,n.mdx)(m,(0,o.A)({},p,r,{components:t,mdxType:"MDXLayout"}),(0,n.mdx)("p",null,"The ",(0,n.mdx)("inlineCode",{parentName:"p"},"vrsplayer"),' app lets you "play" a VRS like a multi-stream video, with audio if an audio stream is present.'),(0,n.mdx)("p",null,(0,n.mdx)("img",{alt:"vrsplayer",src:a(61648).A,width:"1251",height:"634"})),(0,n.mdx)("p",null,"With the ",(0,n.mdx)("inlineCode",{parentName:"p"},"vrsplayer")," app, you can open VRS files that follow the ",(0,n.mdx)("inlineCode",{parentName:"p"},"DataLayout")," conventions described in the Image Support section."),(0,n.mdx)("h2",{id:"playback-controls"},"Playback Controls"),(0,n.mdx)("p",null,"To play/pause/stop playback, use the following controls."),(0,n.mdx)("p",null,(0,n.mdx)("img",{alt:"Playback Controls",src:a(98028).A,width:"874",height:"300"})),(0,n.mdx)("p",null,"The Previous and Next Frame buttons will play at most one frame backward or forward for each visible stream. The Speed controls let you choose to play slower or faster. Obviously, if there is too much data to process for your system, frames will be dropped."),(0,n.mdx)("h2",{id:"overlay-selection"},"Overlay Selection"),(0,n.mdx)("p",null,'The overlay selector lets you choose what information to display over the frames. Choose "Hide" to show nothing, "Tags" to show streams tags, and "Configuration", "State" or "Data" to show the metadata found in the last record of that type.'),(0,n.mdx)("p",null,(0,n.mdx)("img",{alt:"Overlay Control",src:a(63664).A,width:"814",height:"390"})),(0,n.mdx)("p",null,"Use the Text Overlay menubar options to control the color of the text, its font size, and whether the text is drawn on a solid background or not."),(0,n.mdx)("p",null,(0,n.mdx)("img",{alt:"Text Overlay Menu",src:a(99818).A,width:"307",height:"318"})),(0,n.mdx)("p",null,"The ",(0,n.mdx)("inlineCode",{parentName:"p"},"vrsplayer")," app is improved regularly, so it's important to be able to discover functionality directly from the app's user interface. The following sections show less obvious features and controls."),(0,n.mdx)("h2",{id:"tooltips"},"Tooltips"),(0,n.mdx)("p",null,"To know the duration of the image data, use the tooltip found over the time display."),(0,n.mdx)("p",null,"Note that the start and end times show the time range in which image or audio data was found. Streams that don't contain image or audio data are ignored, and only data records from image and audio streams are considered. So if a recording contains a single image stream that has a configuration record at timestamp 0 rather than just before the first data record (as is too often the practice), while the first data record is at timestamp 15, the playback start time will be 15."),(0,n.mdx)("p",null,(0,n.mdx)("img",{alt:"Duration tooltip",src:a(69961).A,width:"729",height:"201"})),(0,n.mdx)("p",null,'The tooltip shown over frames shows the stream\'s ID ("214-1"), the ',(0,n.mdx)("inlineCode",{parentName:"p"},"RecordableTypeId"),' name ("RGB Camera Class"), its flavor (if any, here "device/aria"), and if a stream tag named ',(0,n.mdx)("inlineCode",{parentName:"p"},'"device_type"'),' is found, the device type ("Simulated").'),(0,n.mdx)("p",null,(0,n.mdx)("img",{alt:"Stream tooltip",src:a(92349).A,width:"562",height:"143"})),(0,n.mdx)("h2",{id:"menu-bar-commands"},"Menu Bar Commands"),(0,n.mdx)("p",null,"The Menu Bar offers functionality that's easy to ignore, don't forget to look for more options there!"),(0,n.mdx)("h3",{id:"layout"},"Layout"),(0,n.mdx)("p",null,(0,n.mdx)("img",{alt:"Layout Menu, default",src:a(32034).A,width:"350",height:"162"})),(0,n.mdx)("p",null,"The ",(0,n.mdx)("inlineCode",{parentName:"p"},"Layout")," menu lets you control the number rows and the number of streams shown per row. ",(0,n.mdx)("inlineCode",{parentName:"p"},"Layout Frames 4x2")," means using 2 rows with up to 4 streams each. The layout configurations offered depend on the number of image streams visible."),(0,n.mdx)("h3",{id:"audio"},"Audio"),(0,n.mdx)("p",null,"If your file contains a VRS stream with audio data that contains more than one audio channel, the Audio menu will let you control which audio channel(s) to play. For instance, with an Aria file containing 7 audio channels, the audio menu might look like so:"),(0,n.mdx)("p",null,(0,n.mdx)("img",{alt:"Audio Menu, mono",src:a(54667).A,width:"540",height:"291"})),(0,n.mdx)("p",null,"In this situation, the Mono playback option was selected, so you can choose to play any of the 7 audio channels. If your system supports stereo playback (as most systems do), that channel will be played on both output channels. Use this option to listen to audio channels individually."),(0,n.mdx)("p",null,"Assuming your system supports stereo playback, you can choose one of two stereo channel pairing modes: Auto Channel Pairing, and Manual Channel Pairing."),(0,n.mdx)("p",null,(0,n.mdx)("img",{alt:"Audio Menu, stereo - auto-pairing",src:a(72374).A,width:"550",height:"265"})),(0,n.mdx)("p",null,'Auto Channel Pairing lets you select a first audio channel ("left"), and the next audio channel will automatically be used as the "right" channel. If that automatic selection is not what you need, use the Manual Channel Pairing option:'),(0,n.mdx)("p",null,(0,n.mdx)("img",{alt:"Audio Menu, stereo - manual-pairing",src:a(2387).A,width:"472",height:"301"})),(0,n.mdx)("p",null,'In this mode, you can select arbitrary channels for the "left" and "right" channel of your stereo playback.'),(0,n.mdx)("h3",{id:"presets"},"Presets"),(0,n.mdx)("p",null,(0,n.mdx)("img",{alt:"Presets Menu, save",src:a(40078).A,width:"258",height:"81"})),(0,n.mdx)("p",null,"The ",(0,n.mdx)("inlineCode",{parentName:"p"},"Presets")," menu's top section lets you save and manage presets. Use the ",(0,n.mdx)("inlineCode",{parentName:"p"},"Save Preset")," command to save your favorite stream display and audio configurations, including stream orientation, stream order, and which streams are visible or hidden. To arrange the image streams, you can also use the options presented in the Context Menu section below."),(0,n.mdx)("p",null,(0,n.mdx)("img",{alt:"Presets Menu, delete",src:a(15669).A,width:"452",height:"136"})),(0,n.mdx)("p",null,"Once at least one preset has been saved, you can recall or delete presets, which automatically get a keyboard shortcut for quick access."),(0,n.mdx)("h2",{id:"context-menu"},"Context Menu"),(0,n.mdx)("p",null,"Control the rotation and orientation of each stream using the context menu shown when right-clicking on each stream. The same menu lets you reorder streams, by bumping the chosen stream up or down one position in the order. You can also hide streams from this context menu. The last option lets you save the visible frame as png or jpg file."),(0,n.mdx)("p",null,(0,n.mdx)("img",{alt:"Frame Context Menu",src:a(38095).A,width:"706",height:"516"})),(0,n.mdx)("p",null,"To unhide streams, use one of the ",(0,n.mdx)("inlineCode",{parentName:"p"},"Layout")," menu commands that appear once at least one stream has been hidden."),(0,n.mdx)("p",null,(0,n.mdx)("img",{alt:"Layout Menu, when at least one stream is hidden",src:a(98985).A,width:"359",height:"188"})),(0,n.mdx)("h2",{id:"playback-fps-display"},'Playback "fps" Display'),(0,n.mdx)("p",null,"Normal playback tries to flow at timestamp speed, and frames will be dropped easily at different stages if needed to keep up. At the bottom of each view, you will find an individual fps counter, with 3 parts:"),(0,n.mdx)("p",null,(0,n.mdx)("img",{alt:"fps display",src:a(29355).A,width:"194",height:"58"})),(0,n.mdx)("ul",null,(0,n.mdx)("li",{parentName:"ul"},"the first number (16) is the count of records read from disk per second. During playback, it should be the highest of all the counters. That step includes reading data from storage, lossless decompression, data interpretation and ",(0,n.mdx)("inlineCode",{parentName:"li"},"StreamPlayer")," callbacks processing. Reading files is a single threaded operation (as file APIs typically are), so this work is done by same thread for all streams, which is no decoding or conversion work is done on that thread."),(0,n.mdx)("li",{parentName:"ul"},'the second number (4) is the count of frames "decoded" per second. If images are encoded (jpg/png) they will be decoded. Then if the frames need to be transcoded to a different pixel format for display, they\'ll be converted to a Qt friendly pixel format. If processing is too expensive, frames might be skipped at that stage. That second number can only be equal or lower than the first. This processing is happening on a dedicated thread for each stream.'),(0,n.mdx)("li",{parentName:"ul"},'the last number (2) is the "display" fps ("frames per second"), which is the number of times the code that draws the frame in the Qt widget is called each second. During playback, the number should be equal or lower than the second, but if you resize the windows, that number can go way up, as even if playback is paused and the two first are frozen, this counter will be updated as the window is updated. All draw operations happen in the app\'s single UI thread.')),(0,n.mdx)("p",null,"Putting it all together, for replay, one thread reads the files and extracts the raw image data, one thread draws the processed images in the user interface, but each stream has its own thread to do its image processing independently."),(0,n.mdx)("h2",{id:"keyboard-playback-controls"},"Keyboard Playback Controls"),(0,n.mdx)("p",null,"Playback can be directly controlled from the keyboard:"),(0,n.mdx)("ul",null,(0,n.mdx)("li",{parentName:"ul"},"Use the space bar to play/pause replay."),(0,n.mdx)("li",{parentName:"ul"},"The backspace and the home keys will reset playback to the start of the file, much like the Stop button."),(0,n.mdx)("li",{parentName:"ul"},"The left and right arrow keys will read at most one frame per stream, in either direction."),(0,n.mdx)("li",{parentName:"ul"},"The up and down arrow keys will jump at most 10 frames, in either direction."),(0,n.mdx)("li",{parentName:"ul"},"The page-up and page-down keys will jump at most 100 frames, in either direction.")),(0,n.mdx)("p",null,"When using the arrow keys, all frames are guaranteed to be read. Use this feature if you want to be sure to view every frame of your file."))}u.isMDXComponent=!0},54667:(e,t,a)=>{a.d(t,{A:()=>o});const o=a.p+"assets/images/AudioMenu-Mono-50f54bae208f8b0766be1694577eb723.png"},72374:(e,t,a)=>{a.d(t,{A:()=>o});const o=a.p+"assets/images/AudioMenu-Stereo-AutoPairing-d50ae37b29e4b32c35717030bd4c2a93.png"},2387:(e,t,a)=>{a.d(t,{A:()=>o});const o=a.p+"assets/images/AudioMenu-Stereo-ManualPairing-069012b1e6b101b9b3f11cb261fe7cd6.png"},98028:(e,t,a)=>{a.d(t,{A:()=>o});const o=a.p+"assets/images/Controls-e40ff914647c279ee19fd69612fe812c.png"},69961:(e,t,a)=>{a.d(t,{A:()=>o});const o=a.p+"assets/images/DurationTooltip-4053839eb52580dec7abb09b1b1dca32.png"},29355:(e,t,a)=>{a.d(t,{A:()=>o});const o=a.p+"assets/images/FpsDisplay-b3ffd0f3cd5a6a7d0235c4d5108220f4.png"},38095:(e,t,a)=>{a.d(t,{A:()=>o});const o=a.p+"assets/images/FrameContextMenu-9e9ec43f0e40e387e291790f006d948c.png"},32034:(e,t,a)=>{a.d(t,{A:()=>o});const o=a.p+"assets/images/LayoutMenuDefault-774bce9d9e7dba52a6f1138adf29b92e.png"},98985:(e,t,a)=>{a.d(t,{A:()=>o});const o=a.p+"assets/images/LayoutMenuHiddenStream-3842e52c9968d513e1fab812265dd203.png"},63664:(e,t,a)=>{a.d(t,{A:()=>o});const o=a.p+"assets/images/OverlayPopup-f8686615149c3067af51d51cff4b45c1.png"},15669:(e,t,a)=>{a.d(t,{A:()=>o});const o=a.p+"assets/images/PresetsMenu-008b67eccdf9eb389744f02189892a7a.png"},40078:(e,t,a)=>{a.d(t,{A:()=>o});const o=a.p+"assets/images/PresetsMenuSave-4dd6b1ca38525620f599ce88f176bc7e.png"},92349:(e,t,a)=>{a.d(t,{A:()=>o});const o=a.p+"assets/images/StreamTooltip-a6ce3dad24a416dc215f939bf4605c29.png"},99818:(e,t,a)=>{a.d(t,{A:()=>o});const o=a.p+"assets/images/TextOverlayMenu-0475418447373037fa84b08f6446f889.png"},61648:(e,t,a)=>{a.d(t,{A:()=>o});const o=a.p+"assets/images/vrsplayer-9d93ad375f91b81ff8fd0850c4b25047.png"}}]);