"use strict";(self.webpackChunkwebsite=self.webpackChunkwebsite||[]).push([[72],{86137:(e,t,a)=>{a.r(t),a.d(t,{default:()=>s});var r=a(96540),n=a(20053);const o={tabItem:"tabItem_Ymn6"};function s(e){let{children:t,hidden:a,className:s}=e;return r.createElement("div",{role:"tabpanel",className:(0,n.default)(o.tabItem,s),hidden:a},t)}},57780:(e,t,a)=>{a.r(t),a.d(t,{default:()=>x});var r=a(58168),n=a(96540),o=a(20053),s=a(55236),i=a(56347),l=a(5793),d=a(47422),u=a(81038);function c(e){return function(e){return n.Children.map(e,(e=>{if(!e||(0,n.isValidElement)(e)&&function(e){const{props:t}=e;return!!t&&"object"==typeof t&&"value"in t}(e))return e;throw new Error(`Docusaurus error: Bad <Tabs> child <${"string"==typeof e.type?e.type:e.type.name}>: all children of the <Tabs> component should be <TabItem>, and every <TabItem> should have a unique "value" prop.`)}))?.filter(Boolean)??[]}(e).map((e=>{let{props:{value:t,label:a,attributes:r,default:n}}=e;return{value:t,label:a,attributes:r,default:n}}))}function m(e){const{values:t,children:a}=e;return(0,n.useMemo)((()=>{const e=t??c(a);return function(e){const t=(0,d.X)(e,((e,t)=>e.value===t.value));if(t.length>0)throw new Error(`Docusaurus error: Duplicate values "${t.map((e=>e.value)).join(", ")}" found in <Tabs>. Every value needs to be unique.`)}(e),e}),[t,a])}function p(e){let{value:t,tabValues:a}=e;return a.some((e=>e.value===t))}function f(e){let{queryString:t=!1,groupId:a}=e;const r=(0,i.W6)(),o=function(e){let{queryString:t=!1,groupId:a}=e;if("string"==typeof t)return t;if(!1===t)return null;if(!0===t&&!a)throw new Error('Docusaurus error: The <Tabs> component groupId prop is required if queryString=true, because this value is used as the search param name. You can also provide an explicit value such as queryString="my-search-param".');return a??null}({queryString:t,groupId:a});return[(0,l.aZ)(o),(0,n.useCallback)((e=>{if(!o)return;const t=new URLSearchParams(r.location.search);t.set(o,e),r.replace({...r.location,search:t.toString()})}),[o,r])]}function h(e){const{defaultValue:t,queryString:a=!1,groupId:r}=e,o=m(e),[s,i]=(0,n.useState)((()=>function(e){let{defaultValue:t,tabValues:a}=e;if(0===a.length)throw new Error("Docusaurus error: the <Tabs> component requires at least one <TabItem> children component");if(t){if(!p({value:t,tabValues:a}))throw new Error(`Docusaurus error: The <Tabs> has a defaultValue "${t}" but none of its children has the corresponding value. Available values are: ${a.map((e=>e.value)).join(", ")}. If you intend to show no default tab, use defaultValue={null} instead.`);return t}const r=a.find((e=>e.default))??a[0];if(!r)throw new Error("Unexpected error: 0 tabValues");return r.value}({defaultValue:t,tabValues:o}))),[l,d]=f({queryString:a,groupId:r}),[c,h]=function(e){let{groupId:t}=e;const a=function(e){return e?`docusaurus.tab.${e}`:null}(t),[r,o]=(0,u.Dv)(a);return[r,(0,n.useCallback)((e=>{a&&o.set(e)}),[a,o])]}({groupId:r}),b=(()=>{const e=l??c;return p({value:e,tabValues:o})?e:null})();(0,n.useLayoutEffect)((()=>{b&&i(b)}),[b]);return{selectedValue:s,selectValue:(0,n.useCallback)((e=>{if(!p({value:e,tabValues:o}))throw new Error(`Can't select invalid tab value=${e}`);i(e),d(e),h(e)}),[d,h,o]),tabValues:o}}var b=a(195);const v={tabList:"tabList__CuJ",tabItem:"tabItem_LNqP"};function y(e){let{className:t,block:a,selectedValue:i,selectValue:l,tabValues:d}=e;const u=[],{blockElementScrollPositionUntilNextRender:c}=(0,s.a_)(),m=e=>{const t=e.currentTarget,a=u.indexOf(t),r=d[a].value;r!==i&&(c(t),l(r))},p=e=>{let t=null;switch(e.key){case"Enter":m(e);break;case"ArrowRight":{const a=u.indexOf(e.currentTarget)+1;t=u[a]??u[0];break}case"ArrowLeft":{const a=u.indexOf(e.currentTarget)-1;t=u[a]??u[u.length-1];break}}t?.focus()};return n.createElement("ul",{role:"tablist","aria-orientation":"horizontal",className:(0,o.default)("tabs",{"tabs--block":a},t)},d.map((e=>{let{value:t,label:a,attributes:s}=e;return n.createElement("li",(0,r.A)({role:"tab",tabIndex:i===t?0:-1,"aria-selected":i===t,key:t,ref:e=>u.push(e),onKeyDown:p,onClick:m},s,{className:(0,o.default)("tabs__item",v.tabItem,s?.className,{"tabs__item--active":i===t})}),a??t)})))}function g(e){let{lazy:t,children:a,selectedValue:r}=e;const o=(Array.isArray(a)?a:[a]).filter(Boolean);if(t){const e=o.find((e=>e.props.value===r));return e?(0,n.cloneElement)(e,{className:"margin-top--md"}):null}return n.createElement("div",{className:"margin-top--md"},o.map(((e,t)=>(0,n.cloneElement)(e,{key:t,hidden:e.props.value!==r}))))}function w(e){const t=h(e);return n.createElement("div",{className:(0,o.default)("tabs-container",v.tabList)},n.createElement(y,(0,r.A)({},e,t)),n.createElement(g,(0,r.A)({},e,t)))}function x(e){const t=(0,b.default)();return n.createElement(w,(0,r.A)({key:String(t)},e))}},73387:(e,t,a)=>{a.r(t),a.d(t,{assets:()=>u,contentTitle:()=>l,default:()=>f,frontMatter:()=>i,metadata:()=>d,toc:()=>c});var r=a(58168),n=(a(96540),a(15680)),o=a(57780),s=a(86137);const i={sidebar_position:1,title:"Overview"},l=void 0,d={unversionedId:"Overview",id:"Overview",title:"Overview",description:"VRS is a file format optimized to record and playback streams of sensor data, such as images, audio, and other discrete sensors (IMU, temperature, etc.), that are stored in per-device streams of time-stamped records.",source:"@site/docs/Overview.md",sourceDirName:".",slug:"/Overview",permalink:"/vrs/docs/Overview",draft:!1,editUrl:"https://github.com/facebookresearch/vrs/edit/main/website/docs/Overview.md",tags:[],version:"current",sidebarPosition:1,frontMatter:{sidebar_position:1,title:"Overview"},sidebar:"tutorialSidebar",next:{title:"Organization",permalink:"/vrs/docs/Organization"}},u={},c=[{value:"Appropriate Use Cases",id:"appropriate-use-cases",level:2},{value:"Data Types and Data Conventions",id:"data-types-and-data-conventions",level:2},{value:"Features Overview",id:"features-overview",level:2}],m={toc:c},p="wrapper";function f(e){let{components:t,...a}=e;return(0,n.mdx)(p,(0,r.A)({},m,a,{components:t,mdxType:"MDXLayout"}),(0,n.mdx)("p",null,"VRS is a file format optimized to record and playback streams of sensor data, such as images, audio, and other discrete sensors (IMU, temperature, etc.), that are stored in per-device streams of time-stamped records."),(0,n.mdx)("p",null,"VRS was first created to record images and sensor data produced by early prototypes of the ",(0,n.mdx)("a",{parentName:"p",href:"https://store.facebook.com/quest/products/quest-2"},"Meta Quest device"),", to develop the device\u2019s positional tracking system known as ",(0,n.mdx)("a",{parentName:"p",href:"https://ai.facebook.com/blog/powered-by-ai-oculus-insight/"},"Oculus Insight"),", ",(0,n.mdx)("a",{parentName:"p",href:"https://www.meta.com/help/quest/articles/headsets-and-accessories/controllers-and-hand-tracking/hand-tracking/"},"Meta Quest 2's hand tracking"),", and more to come."),(0,n.mdx)("p",null,(0,n.mdx)("a",{parentName:"p",href:"https://about.facebook.com/realitylabs/projectaria/"},"Project Aria")," records its data as VRS files."),(0,n.mdx)("h2",{id:"appropriate-use-cases"},"Appropriate Use Cases"),(0,n.mdx)("p",null,"VRS is designed to record similar looking bundles of data produced repeatedly over a period of time, in time-stamped records."),(0,n.mdx)(o.default,{mdxType:"Tabs"},(0,n.mdx)(s.default,{value:"good_cases",label:"Good Use Cases",default:!0,mdxType:"TabItem"},"VRS is very well suited to record and playback:",(0,n.mdx)("li",null,"Data produced by the cameras and sensors of a Meta Quest device, including IMUs."),(0,n.mdx)("li",null,"Data produced by the cameras and sensors of Project Aria Glasses, including positional tracking cameras, eye tracking cameras, barometer sensors, GPS and BT beacons, multi-channel audio."),(0,n.mdx)("li",null,"Data produced in burst of activity, such as keyboard or mouse input data."),(0,n.mdx)("li",null,"TCP/IP or USB packets.")),(0,n.mdx)(s.default,{value:"bad_cases",label:"Poor Use Cases",mdxType:"TabItem"},"Even if VRS could be used to store pretty much any data, VRS is a poor choice to record anything without temporal information or data format regularity, such as:",(0,n.mdx)("li",null,"Text documents."),(0,n.mdx)("li",null,"Web pages."),(0,n.mdx)("li",null,"Point cloud data."),(0,n.mdx)("li",null,"3D models."),(0,n.mdx)("li",null,"Single images with annotations, unless you bundle many images and annotations in a single VRS file."))),(0,n.mdx)("admonition",{type:"info"},(0,n.mdx)("p",{parentName:"admonition"},"While VRS is very effective at streaming very large amounts of data to disk, potentially to and from cloud storage, with real-time lossless compression, creating files potentially very large (typical VRS files range from 5 to 80 GB, but 1.5 TB VRS files exist), editing VRS files is not nearly as convenient, as the entire container typically needs to be rewritten.")),(0,n.mdx)("h2",{id:"data-types-and-data-conventions"},"Data Types and Data Conventions"),(0,n.mdx)("p",null,"VRS provides standardized methods to store images, audio, and discrete sensors data in compact and format evolution resilient records, so you can save data without having to worry too much about evolving requirements. But while VRS standardizes how to save common data types, VRS does not prescribe how to address specific use cases. Data format conventions are desirable, to enable teams working on identical or similar use cases to exchange data. However, such data format conventions are out of the scope of VRS."),(0,n.mdx)("h2",{id:"features-overview"},"Features Overview"),(0,n.mdx)("ul",null,(0,n.mdx)("li",{parentName:"ul"},"VRS files contain multiple streams of time-sorted records generated by a set of sensors (camera, IMU, thermometer, GPS, etc), typically one set of sensors per stream."),(0,n.mdx)("li",{parentName:"ul"},"The file and each stream contain an independent set of tags, which are string name/value pairs that describe them."),(0,n.mdx)("li",{parentName:"ul"},"Streams may contain ",(0,n.mdx)("inlineCode",{parentName:"li"},"Configuration"),", ",(0,n.mdx)("inlineCode",{parentName:"li"},"State")," and ",(0,n.mdx)("inlineCode",{parentName:"li"},"Data")," records, each with a timestamp in a common time domain for the whole file. Typically, streams contain one ",(0,n.mdx)("inlineCode",{parentName:"li"},"Configuration")," record and one ",(0,n.mdx)("inlineCode",{parentName:"li"},"State")," record, followed by one to millions of ",(0,n.mdx)("inlineCode",{parentName:"li"},"Data")," records."),(0,n.mdx)("li",{parentName:"ul"},"Records are structured as a succession of typed content blocks. Typically, content blocks are metadata, image, audio, and custom content blocks."),(0,n.mdx)("li",{parentName:"ul"},"Metadata content blocks contain raw sensor data described once per stream, making the file format very efficient. The marginal cost of adding 1 byte of data to each metadata content block in a stream is 1 byte per record (or less when lossless compression happens)."),(0,n.mdx)("li",{parentName:"ul"},"Records can be losslessly compressed using lz4 or zstd, which can be fast enough to compress while recording on device."),(0,n.mdx)("li",{parentName:"ul"},"Multiple threads can create records concurrently for the same file, without CPU contention."),(0,n.mdx)("li",{parentName:"ul"},"VRS supports huge file sizes (tested with multi-terabyte use cases)."),(0,n.mdx)("li",{parentName:"ul"},"VRS supports chunked files: auto-chunking on creation and automated chunk detection for playback."),(0,n.mdx)("li",{parentName:"ul"},"Playback is optimized for timestamp order, which is key for network streaming."),(0,n.mdx)("li",{parentName:"ul"},"Random-access playback is supported."),(0,n.mdx)("li",{parentName:"ul"},"Custom ",(0,n.mdx)("inlineCode",{parentName:"li"},"FileHandler")," implementations can add support for cloud storage streaming.")))}f.isMDXComponent=!0}}]);