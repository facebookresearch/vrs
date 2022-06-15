"use strict";(self.webpackChunkwebsite=self.webpackChunkwebsite||[]).push([[514,75],{3905:function(e,t,n){n.r(t),n.d(t,{MDXContext:function(){return s},MDXProvider:function(){return m},mdx:function(){return h},useMDXComponents:function(){return d},withMDXComponents:function(){return u}});var r=n(67294);function a(e,t,n){return t in e?Object.defineProperty(e,t,{value:n,enumerable:!0,configurable:!0,writable:!0}):e[t]=n,e}function o(){return o=Object.assign||function(e){for(var t=1;t<arguments.length;t++){var n=arguments[t];for(var r in n)Object.prototype.hasOwnProperty.call(n,r)&&(e[r]=n[r])}return e},o.apply(this,arguments)}function l(e,t){var n=Object.keys(e);if(Object.getOwnPropertySymbols){var r=Object.getOwnPropertySymbols(e);t&&(r=r.filter((function(t){return Object.getOwnPropertyDescriptor(e,t).enumerable}))),n.push.apply(n,r)}return n}function i(e){for(var t=1;t<arguments.length;t++){var n=null!=arguments[t]?arguments[t]:{};t%2?l(Object(n),!0).forEach((function(t){a(e,t,n[t])})):Object.getOwnPropertyDescriptors?Object.defineProperties(e,Object.getOwnPropertyDescriptors(n)):l(Object(n)).forEach((function(t){Object.defineProperty(e,t,Object.getOwnPropertyDescriptor(n,t))}))}return e}function c(e,t){if(null==e)return{};var n,r,a=function(e,t){if(null==e)return{};var n,r,a={},o=Object.keys(e);for(r=0;r<o.length;r++)n=o[r],t.indexOf(n)>=0||(a[n]=e[n]);return a}(e,t);if(Object.getOwnPropertySymbols){var o=Object.getOwnPropertySymbols(e);for(r=0;r<o.length;r++)n=o[r],t.indexOf(n)>=0||Object.prototype.propertyIsEnumerable.call(e,n)&&(a[n]=e[n])}return a}var s=r.createContext({}),u=function(e){return function(t){var n=d(t.components);return r.createElement(e,o({},t,{components:n}))}},d=function(e){var t=r.useContext(s),n=t;return e&&(n="function"==typeof e?e(t):i(i({},t),e)),n},m=function(e){var t=d(e.components);return r.createElement(s.Provider,{value:t},e.children)},p={inlineCode:"code",wrapper:function(e){var t=e.children;return r.createElement(r.Fragment,{},t)}},f=r.forwardRef((function(e,t){var n=e.components,a=e.mdxType,o=e.originalType,l=e.parentName,s=c(e,["components","mdxType","originalType","parentName"]),u=d(n),m=a,f=u["".concat(l,".").concat(m)]||u[m]||p[m]||o;return n?r.createElement(f,i(i({ref:t},s),{},{components:n})):r.createElement(f,i({ref:t},s))}));function h(e,t){var n=arguments,a=t&&t.mdxType;if("string"==typeof e||a){var o=n.length,l=new Array(o);l[0]=f;var i={};for(var c in t)hasOwnProperty.call(t,c)&&(i[c]=t[c]);i.originalType=e,i.mdxType="string"==typeof e?e:a,l[1]=i;for(var s=2;s<o;s++)l[s]=n[s];return r.createElement.apply(null,l)}return r.createElement.apply(null,n)}f.displayName="MDXCreateElement"},42628:function(e,t,n){n.r(t),n.d(t,{default:function(){return y}});var r=n(87462),a=n(67294),o=n(86010),l=n(23746);var i=n(95999),c=n(63616),s={plain:{color:"#bfc7d5",backgroundColor:"#292d3e"},styles:[{types:["comment"],style:{color:"rgb(105, 112, 152)",fontStyle:"italic"}},{types:["string","inserted"],style:{color:"rgb(195, 232, 141)"}},{types:["number"],style:{color:"rgb(247, 140, 108)"}},{types:["builtin","char","constant","function"],style:{color:"rgb(130, 170, 255)"}},{types:["punctuation","selector"],style:{color:"rgb(199, 146, 234)"}},{types:["variable"],style:{color:"rgb(191, 199, 213)"}},{types:["class-name","attr-name"],style:{color:"rgb(255, 203, 107)"}},{types:["tag","deleted"],style:{color:"rgb(255, 85, 114)"}},{types:["operator"],style:{color:"rgb(137, 221, 255)"}},{types:["boolean"],style:{color:"rgb(255, 88, 116)"}},{types:["keyword"],style:{fontStyle:"italic"}},{types:["doctype"],style:{color:"rgb(199, 146, 234)",fontStyle:"italic"}},{types:["namespace"],style:{color:"rgb(178, 204, 214)"}},{types:["url"],style:{color:"rgb(221, 221, 221)"}}]},u=n(85350),d=function(){var e=(0,c.useThemeConfig)().prism,t=(0,u.Z)().isDarkTheme,n=e.theme||s,r=e.darkTheme||n;return t?r:n},m="codeBlockContainer_J+bg",p="codeBlockContent_csEI",f="codeBlockTitle_oQzk",h="codeBlock_rtdJ",g="copyButton_M3SB",b="codeBlockLines_1zSZ";function y(e){var t,n=e.children,s=e.className,u=e.metastring,y=e.title,A=(0,c.useThemeConfig)().prism,v=(0,a.useState)(!1),E=v[0],k=v[1],C=(0,a.useState)(!1),N=C[0],T=C[1];(0,a.useEffect)((function(){T(!0)}),[]);var w=(0,c.parseCodeBlockTitle)(u)||y,O=d(),S=Array.isArray(n)?n.join(""):n,I=null!=(t=(0,c.parseLanguage)(s))?t:A.defaultLanguage,P=(0,c.parseLines)(S,u,I),j=P.highlightLines,B=P.code,L=function(){!function(e,t){var n=(void 0===t?{}:t).target,r=void 0===n?document.body:n,a=document.createElement("textarea"),o=document.activeElement;a.value=e,a.setAttribute("readonly",""),a.style.contain="strict",a.style.position="absolute",a.style.left="-9999px",a.style.fontSize="12pt";var l=document.getSelection(),i=!1;l.rangeCount>0&&(i=l.getRangeAt(0)),r.append(a),a.select(),a.selectionStart=0,a.selectionEnd=e.length;var c=!1;try{c=document.execCommand("copy")}catch(s){}a.remove(),i&&(l.removeAllRanges(),l.addRange(i)),o&&o.focus()}(B),k(!0),setTimeout((function(){return k(!1)}),2e3)};return a.createElement(l.ZP,(0,r.Z)({},l.lG,{key:String(N),theme:O,code:B,language:I}),(function(e){var t=e.className,n=e.style,l=e.tokens,u=e.getLineProps,d=e.getTokenProps;return a.createElement("div",{className:(0,o.default)(m,s,c.ThemeClassNames.common.codeBlock)},w&&a.createElement("div",{style:n,className:f},w),a.createElement("div",{className:(0,o.default)(p,I)},a.createElement("pre",{tabIndex:0,className:(0,o.default)(t,h,"thin-scrollbar"),style:n},a.createElement("code",{className:b},l.map((function(e,t){1===e.length&&"\n"===e[0].content&&(e[0].content="");var n=u({line:e,key:t});return j.includes(t)&&(n.className+=" docusaurus-highlight-code-line"),a.createElement("span",(0,r.Z)({key:t},n),e.map((function(e,t){return a.createElement("span",(0,r.Z)({key:t},d({token:e,key:t})))})),a.createElement("br",null))})))),a.createElement("button",{type:"button","aria-label":(0,i.translate)({id:"theme.CodeBlock.copyButtonAriaLabel",message:"Copy code to clipboard",description:"The ARIA label for copy code blocks button"}),className:(0,o.default)(g,"clean-btn"),onClick:L},E?a.createElement(i.default,{id:"theme.CodeBlock.copied",description:"The copied button label on code blocks"},"Copied"):a.createElement(i.default,{id:"theme.CodeBlock.copy",description:"The copy button label on code blocks"},"Copy"))))}))}},85642:function(e,t,n){n.r(t),n.d(t,{default:function(){return te}});var r=n(67294),a=n(3905),o=n(46291),l=n(74161),i=n(86010),c=n(63616),s=n(93783),u=n(55537),d=n(87462);var m=function(e){return r.createElement("svg",(0,d.Z)({width:"20",height:"20","aria-hidden":"true"},e),r.createElement("g",{fill:"#7a7a7a"},r.createElement("path",{d:"M9.992 10.023c0 .2-.062.399-.172.547l-4.996 7.492a.982.982 0 01-.828.454H1c-.55 0-1-.453-1-1 0-.2.059-.403.168-.551l4.629-6.942L.168 3.078A.939.939 0 010 2.528c0-.548.45-.997 1-.997h2.996c.352 0 .649.18.828.45L9.82 9.472c.11.148.172.347.172.55zm0 0"}),r.createElement("path",{d:"M19.98 10.023c0 .2-.058.399-.168.547l-4.996 7.492a.987.987 0 01-.828.454h-3c-.547 0-.996-.453-.996-1 0-.2.059-.403.168-.551l4.625-6.942-4.625-6.945a.939.939 0 01-.168-.55 1 1 0 01.996-.997h3c.348 0 .649.18.828.45l4.996 7.492c.11.148.168.347.168.55zm0 0"})))},p=n(95999),f=n(63366),h=n(39960),g=n(13919),b=n(90541),y="menuLinkText_OKON",A="hasHref_TwRn",v=n(72389),E=["items"],k=["item"],C=["item","onItemClick","activePath","level"],N=["item","onItemClick","activePath","level"],T=(0,r.memo)((function(e){var t=e.items,n=(0,f.Z)(e,E);return r.createElement(r.Fragment,null,t.map((function(e,t){return r.createElement(w,(0,d.Z)({key:t,item:e},n))})))}));function w(e){var t=e.item,n=(0,f.Z)(e,k);return"category"===t.type?0===t.items.length?null:r.createElement(O,(0,d.Z)({item:t},n)):r.createElement(S,(0,d.Z)({item:t},n))}function O(e){var t,n=e.item,a=e.onItemClick,o=e.activePath,l=e.level,s=(0,f.Z)(e,C),u=n.items,m=n.label,g=n.collapsible,b=n.className,E=n.href,k=function(e){var t=(0,v.default)();return(0,r.useMemo)((function(){return e.href?e.href:!t&&e.collapsible?(0,c.findFirstCategoryLink)(e):void 0}),[e,t])}(n),N=(0,c.isActiveSidebarItem)(n,o),w=(0,c.useCollapsible)({initialState:function(){return!!g&&(!N&&n.collapsed)}}),O=w.collapsed,S=w.setCollapsed,I=w.toggleCollapsed;return function(e){var t=e.isActive,n=e.collapsed,a=e.setCollapsed,o=(0,c.usePrevious)(t);(0,r.useEffect)((function(){t&&!o&&n&&a(!1)}),[t,o,n,a])}({isActive:N,collapsed:O,setCollapsed:S}),r.createElement("li",{className:(0,i.default)(c.ThemeClassNames.docs.docSidebarItemCategory,c.ThemeClassNames.docs.docSidebarItemCategoryLevel(l),"menu__list-item",{"menu__list-item--collapsed":O},b)},r.createElement("div",{className:"menu__list-item-collapsible"},r.createElement(h.default,(0,d.Z)({className:(0,i.default)("menu__link",(t={"menu__link--sublist":g&&!E,"menu__link--active":N},t[y]=!g,t[A]=!!k,t)),onClick:g?function(e){null==a||a(n),E?S(!1):(e.preventDefault(),I())}:function(){null==a||a(n)},href:g?null!=k?k:"#":k},s),m),E&&g&&r.createElement("button",{"aria-label":(0,p.translate)({id:"theme.DocSidebarItem.toggleCollapsedCategoryAriaLabel",message:"Toggle the collapsible sidebar category '{label}'",description:"The ARIA label to toggle the collapsible sidebar category"},{label:m}),type:"button",className:"clean-btn menu__caret",onClick:function(e){e.preventDefault(),I()}})),r.createElement(c.Collapsible,{lazy:!0,as:"ul",className:"menu__list",collapsed:O},r.createElement(T,{items:u,tabIndex:O?-1:0,onItemClick:a,activePath:o,level:l+1})))}function S(e){var t=e.item,n=e.onItemClick,a=e.activePath,o=e.level,l=(0,f.Z)(e,N),s=t.href,u=t.label,m=t.className,p=(0,c.isActiveSidebarItem)(t,a);return r.createElement("li",{className:(0,i.default)(c.ThemeClassNames.docs.docSidebarItemLink,c.ThemeClassNames.docs.docSidebarItemLinkLevel(o),"menu__list-item",m),key:u},r.createElement(h.default,(0,d.Z)({className:(0,i.default)("menu__link",{"menu__link--active":p}),"aria-current":p?"page":void 0,to:s},(0,g.Z)(s)&&{onClick:n?function(){return n(t)}:void 0},l),(0,g.Z)(s)?u:r.createElement("span",null,u,r.createElement(b.Z,null))))}var I="sidebar_a3j0",P="sidebarWithHideableNavbar_VlPv",j="sidebarHidden_OqfG",B="sidebarLogo_hmkv",L="menu_cyFh",Z="menuWithAnnouncementBar_+O1J",M="collapseSidebarButton_eoK2",x="collapseSidebarButtonIcon_e+kA";function F(e){var t=e.onClick;return r.createElement("button",{type:"button",title:(0,p.translate)({id:"theme.docs.sidebar.collapseButtonTitle",message:"Collapse sidebar",description:"The title attribute for collapse button of doc sidebar"}),"aria-label":(0,p.translate)({id:"theme.docs.sidebar.collapseButtonAriaLabel",message:"Collapse sidebar",description:"The title attribute for collapse button of doc sidebar"}),className:(0,i.default)("button button--secondary button--outline",M),onClick:t},r.createElement(m,{className:x}))}function W(e){var t,n,a=e.path,o=e.sidebar,l=e.onCollapse,s=e.isHidden,d=function(){var e=(0,c.useAnnouncementBar)().isActive,t=(0,r.useState)(e),n=t[0],a=t[1];return(0,c.useScrollPosition)((function(t){var n=t.scrollY;e&&a(0===n)}),[e]),e&&n}(),m=(0,c.useThemeConfig)(),p=m.navbar.hideOnScroll,f=m.hideableSidebar;return r.createElement("div",{className:(0,i.default)(I,(t={},t[P]=p,t[j]=s,t))},p&&r.createElement(u.Z,{tabIndex:-1,className:B}),r.createElement("nav",{className:(0,i.default)("menu thin-scrollbar",L,(n={},n[Z]=d,n))},r.createElement("ul",{className:(0,i.default)(c.ThemeClassNames.docs.docSidebarMenu,"menu__list")},r.createElement(T,{items:o,activePath:a,level:1}))),f&&r.createElement(F,{onClick:l}))}var z=function(e){var t=e.toggleSidebar,n=e.sidebar,a=e.path;return r.createElement("ul",{className:(0,i.default)(c.ThemeClassNames.docs.docSidebarMenu,"menu__list")},r.createElement(T,{items:n,activePath:a,onItemClick:function(e){"category"===e.type&&e.href&&t(),"link"===e.type&&t()},level:1}))};function U(e){return r.createElement(c.MobileSecondaryMenuFiller,{component:z,props:e})}var D=r.memo(W),R=r.memo(U);function G(e){var t=(0,s.Z)(),n="desktop"===t||"ssr"===t,a="mobile"===t;return r.createElement(r.Fragment,null,n&&r.createElement(D,e),a&&r.createElement(R,e))}var Y=n(75854),H=n.n(Y),X=n(24608),J="backToTopButton_i9tI",V="backToTopButtonShow_wCmF";function Q(){var e=(0,r.useRef)(null);return{smoothScrollTop:function(){var t;e.current=(t=null,function e(){var n=document.documentElement.scrollTop;n>0&&(t=requestAnimationFrame(e),window.scrollTo(0,Math.floor(.85*n)))}(),function(){return t&&cancelAnimationFrame(t)})},cancelScrollToTop:function(){return null==e.current?void 0:e.current()}}}var q=function(){var e,t=(0,r.useState)(!1),n=t[0],a=t[1],o=(0,r.useRef)(!1),l=Q(),s=l.smoothScrollTop,u=l.cancelScrollToTop;return(0,c.useScrollPosition)((function(e,t){var n=e.scrollY,r=null==t?void 0:t.scrollY;if(r)if(o.current)o.current=!1;else{var l=n<r;if(l||u(),n<300)a(!1);else if(l){var i=document.documentElement.scrollHeight;n+window.innerHeight<i&&a(!0)}else a(!1)}})),(0,c.useLocationChange)((function(e){e.location.hash&&(o.current=!0,a(!1))})),r.createElement("button",{"aria-label":(0,p.translate)({id:"theme.BackToTopButton.buttonAriaLabel",message:"Scroll back to top",description:"The ARIA label for the back to top button"}),className:(0,i.default)("clean-btn",c.ThemeClassNames.common.backToTopButton,J,(e={},e[V]=n,e)),type:"button",onClick:function(){return s()}})},_=n(76775),K={docPage:"docPage_lDyR",docMainContainer:"docMainContainer_r8cw",docSidebarContainer:"docSidebarContainer_0YBq",docMainContainerEnhanced:"docMainContainerEnhanced_SOUu",docSidebarContainerHidden:"docSidebarContainerHidden_Qlt2",collapsedDocSidebar:"collapsedDocSidebar_zZpm",expandSidebarButtonIcon:"expandSidebarButtonIcon_cxi8",docItemWrapperEnhanced:"docItemWrapperEnhanced_aT5H"},$=n(12859);function ee(e){var t,n,o,s=e.currentDocRoute,u=e.versionMetadata,d=e.children,f=e.sidebarName,h=(0,c.useDocsSidebar)(),g=u.pluginId,b=u.version,y=(0,r.useState)(!1),A=y[0],v=y[1],E=(0,r.useState)(!1),k=E[0],C=E[1],N=(0,r.useCallback)((function(){k&&C(!1),v((function(e){return!e}))}),[k]);return r.createElement(l.Z,{wrapperClassName:c.ThemeClassNames.wrapper.docsPages,pageClassName:c.ThemeClassNames.page.docsDocPage,searchMetadata:{version:b,tag:(0,c.docVersionSearchTag)(g,b)}},r.createElement("div",{className:K.docPage},r.createElement(q,null),h&&r.createElement("aside",{className:(0,i.default)(K.docSidebarContainer,(t={},t[K.docSidebarContainerHidden]=A,t)),onTransitionEnd:function(e){e.currentTarget.classList.contains(K.docSidebarContainer)&&A&&C(!0)}},r.createElement(G,{key:f,sidebar:h,path:s.path,onCollapse:N,isHidden:k}),k&&r.createElement("div",{className:K.collapsedDocSidebar,title:(0,p.translate)({id:"theme.docs.sidebar.expandButtonTitle",message:"Expand sidebar",description:"The ARIA label and title attribute for expand button of doc sidebar"}),"aria-label":(0,p.translate)({id:"theme.docs.sidebar.expandButtonAriaLabel",message:"Expand sidebar",description:"The ARIA label and title attribute for expand button of doc sidebar"}),tabIndex:0,role:"button",onKeyDown:N,onClick:N},r.createElement(m,{className:K.expandSidebarButtonIcon}))),r.createElement("main",{className:(0,i.default)(K.docMainContainer,(n={},n[K.docMainContainerEnhanced]=A||!h,n))},r.createElement("div",{className:(0,i.default)("container padding-top--md padding-bottom--lg",K.docItemWrapper,(o={},o[K.docItemWrapperEnhanced]=A,o))},r.createElement(a.MDXProvider,{components:H()},d)))))}var te=function(e){var t=e.route.routes,n=e.versionMetadata,a=e.location,l=t.find((function(e){return(0,_.LX)(a.pathname,e)}));if(!l)return r.createElement(X.default,null);var i=l.sidebar,s=i?n.docsSidebars[i]:null;return r.createElement(r.Fragment,null,r.createElement($.Z,null,r.createElement("html",{className:n.className})),r.createElement(c.DocsVersionProvider,{version:n},r.createElement(c.DocsSidebarProvider,{sidebar:s},r.createElement(ee,{currentDocRoute:l,versionMetadata:n,sidebarName:i},(0,o.Z)(t,{versionMetadata:n})))))}},17642:function(e,t,n){n.r(t),n.d(t,{default:function(){return E}});var r=n(87462),a=n(63366),o=n(67294),l=n(12859),i=n(39960),c=n(20625),s=n.n(c),u=n(86010),d=n(95999),m=n(63616),p="anchorWithStickyNavbar_y2LR",f="anchorWithHideOnScrollNavbar_3ly5",h=["id"],g=function(e){var t=Object.assign({},e);return o.createElement("header",null,o.createElement("h1",(0,r.Z)({},t,{id:void 0}),t.children))},b=function(e){return"h1"===e?g:(t=e,function(e){var n,l=e.id,i=(0,a.Z)(e,h),c=(0,m.useThemeConfig)().navbar.hideOnScroll;return l?o.createElement(t,(0,r.Z)({},i,{className:(0,u.default)("anchor",(n={},n[f]=c,n[p]=!c,n)),id:l}),i.children,o.createElement("a",{className:"hash-link",href:"#"+l,title:(0,d.translate)({id:"theme.common.headingLinkTitle",message:"Direct link to heading",description:"Title for link to heading"})},"\u200b")):o.createElement(t,i)});var t},y="details_h+cY";function A(e){var t=Object.assign({},e);return o.createElement(m.Details,(0,r.Z)({},t,{className:(0,u.default)("alert alert--info",y,t.className)}))}var v=["mdxType","originalType"];var E={head:function(e){var t=o.Children.map(e.children,(function(e){return function(e){var t,n;if(null!=e&&null!=(t=e.props)&&t.mdxType&&null!=e&&null!=(n=e.props)&&n.originalType){var r=e.props,l=(r.mdxType,r.originalType,(0,a.Z)(r,v));return o.createElement(e.props.originalType,l)}return e}(e)}));return o.createElement(l.Z,e,t)},code:function(e){var t=e.children;return(0,o.isValidElement)(t)?t:t.includes("\n")?o.createElement(s(),e):o.createElement("code",e)},a:function(e){return o.createElement(i.default,e)},pre:function(e){var t,n=e.children;return(0,o.isValidElement)(n)&&(0,o.isValidElement)(null==n||null==(t=n.props)?void 0:t.children)?n.props.children:o.createElement(s(),(0,o.isValidElement)(n)?null==n?void 0:n.props:Object.assign({},e))},details:function(e){var t=o.Children.toArray(e.children),n=t.find((function(e){var t;return"summary"===(null==e||null==(t=e.props)?void 0:t.mdxType)})),a=o.createElement(o.Fragment,null,t.filter((function(e){return e!==n})));return o.createElement(A,(0,r.Z)({},e,{summary:n}),a)},h1:b("h1"),h2:b("h2"),h3:b("h3"),h4:b("h4"),h5:b("h5"),h6:b("h6")}},24608:function(e,t,n){n.r(t);var r=n(67294),a=n(74161),o=n(95999);t.default=function(){return r.createElement(a.Z,{title:(0,o.translate)({id:"theme.NotFound.title",message:"Page Not Found"})},r.createElement("main",{className:"container margin-vert--xl"},r.createElement("div",{className:"row"},r.createElement("div",{className:"col col--6 col--offset-3"},r.createElement("h1",{className:"hero__title"},r.createElement(o.default,{id:"theme.NotFound.title",description:"The title of the 404 page"},"Page Not Found")),r.createElement("p",null,r.createElement(o.default,{id:"theme.NotFound.p1",description:"The first paragraph of the 404 page"},"We could not find what you were looking for.")),r.createElement("p",null,r.createElement(o.default,{id:"theme.NotFound.p2",description:"The 2nd paragraph of the 404 page"},"Please contact the owner of the site that linked you to the original URL and let them know their link is broken."))))))}},14732:function(e,t){Object.defineProperty(t,"__esModule",{value:!0});t.default="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAAAXNSR0IArs4c6QAAAIRlWElmTU0AKgAAAAgABQESAAMAAAABAAEAAAEaAAUAAAABAAAASgEbAAUAAAABAAAAUgEoAAMAAAABAAIAAIdpAAQAAAABAAAAWgAAAAAAAABIAAAAAQAAAEgAAAABAAOgAQADAAAAAQABAACgAgAEAAAAAQAAACCgAwAEAAAAAQAAACAAAAAAX7wP8AAAAAlwSFlzAAALEwAACxMBAJqcGAAAAVlpVFh0WE1MOmNvbS5hZG9iZS54bXAAAAAAADx4OnhtcG1ldGEgeG1sbnM6eD0iYWRvYmU6bnM6bWV0YS8iIHg6eG1wdGs9IlhNUCBDb3JlIDUuNC4wIj4KICAgPHJkZjpSREYgeG1sbnM6cmRmPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5LzAyLzIyLXJkZi1zeW50YXgtbnMjIj4KICAgICAgPHJkZjpEZXNjcmlwdGlvbiByZGY6YWJvdXQ9IiIKICAgICAgICAgICAgeG1sbnM6dGlmZj0iaHR0cDovL25zLmFkb2JlLmNvbS90aWZmLzEuMC8iPgogICAgICAgICA8dGlmZjpPcmllbnRhdGlvbj4xPC90aWZmOk9yaWVudGF0aW9uPgogICAgICA8L3JkZjpEZXNjcmlwdGlvbj4KICAgPC9yZGY6UkRGPgo8L3g6eG1wbWV0YT4KTMInWQAACexJREFUWAmdV2tsXMUV/uY+d9d3/YqDHUKKSYLzDiIpUAEFB9EUWhApiYOaIgjQJog/ULVQVQVpS6nUltJUiNLmoZZfVMS0lGdBPOzmgSolEIVgEsvkYRLb8Sv2rnfv7t7X9JxZ72YNVJV6tbt37p2Z833nO2fOzAr8jyslU9rSzh6xcWNnyEOllNr2Pfcv8CL/4hBBfWm6MRnXzP6t1/3puBAi4ncduzt0vndOz+P2l13iy16W323fvsXcunWHz89/6P7BFYUwf08U+d8IZTjfjGmaEFINjSIJrxBFGrQTmqa/bZnxvzzU/twB7tyyfbW5Y+sHyoYa/Lmf/0ZApLra9dSa7mDX+w8sTOfT26AHt+iWICAfga/ECCXpwR+AqUhdNwUsW0dQpKdQfz0O54cPrn2uj2wZZIsnlRhXkfgyAiIlIVIC0e+77r3PD3I7zQREPutHEAgRQYeQDKjmUkhK5gS1InpJY6Io0mOOoXmulKYW2/LI2hd2kU0txQTETBJaFRnVpNhpDL7tvc2PGwl/VygDUciFPsWWx5pkQCNIMkP4lS/1MDT3ESbfi27oRzRXd/ydv3prwy9SZLOjs+MLeCpRyiRYqmdveSPc1nX3j61k9ER2Mh8Aao5RHlO6E7hOgujcHVIACL8kSPUwsq1FxaIfJmq19vaNy9ynb3txH+fEB68NqUTlwUpGbkzHKXj6vXuuj0yvu+B6EftUkrokM7VZRHI2QuQmIIs2rwrSpQjdyUJGbI5HlcazXWpLRFJaCUPTfLv9kbWd/ypjcX+ZgJpJxsRv3/3uMc2UbUExCmjytOfnjcqQWJk+Mh/PgduTJCXISMsk6lYNwrJMgqNEmEmAWMjQsIQeeqLv0ZtfXUTh5CEcMqn0JUYqFE+9d+dW29Ha/GLol8DZk/PecJPMQDNIfiOENCkEFq0wahfcEF4xUOFgzz536QEVjlitfukT/7ztfu5LdZcwZyjw67c3HCamK8l7WmLsW0lQviseWoQwnUThdAOK52hJjmvqtZ4MYNZSWJvOId6ShklLkUNTUUKtFFLB1kgF+dFjN79xGVlUqmvTFUv+5t07VgkdK70irSJa06Xp095XiZD5pA7pIyaKE6TfrAxA3yArkD9loXiyHvmMhF8gfcuuKXDFXydlpdDFyl++eesqdomxtWWzR9VQGchr7ZjBgSFtpz0u39UjUSInI/rRYgQwbxTxxQNILBmAcckYhYFyjaizY/lsRAXrPImyEhT7gCooqJJeSwPRMEGFkxt8RSJYwelDAGXupQ76LRtg0ciISjSzMUc9BpEyYDZRm8LDWcX5RV6i6EqEngFDj0EXlrJFYijbRHQFv7iwLSmNpaOzlcAyilrCgChUx05Nq/qhvtBjH6kYZhsgnGEldTjeBBFR3GmF0LZA1YjJ6DiX7gfsPtixZtTobcqQwohkCz8wttE5bT8et2w7YYCCT9dMEUoMyTDJnG/NYLDfR21/LZCLqWj5oyZcN4fkgiycJBdCE/lwBDfN+wnmNbYh441j/9B2MusJu8Yi3TwlCWNrHR0lBqatuXZMpwzW6Esmqr4WtWNxGwUcx7dv2Ii7Nt+JwbMD8IZj8IZsDI8OY936dbhpzTp44hTiiTh0q4BFcy/H8otXYf6sZQjCAqyYIe24BsPW84zK2MYn3aUkpLicUXVPBZx9ZhWU79TS4EUZNJlXY1HD1XDmJpH3snjh+Reh6zrW374BV37tq0qFo2PXIB18inp7CWosWrJeEWPZIbj+AKzipVI6ZFNGp5kAY59PwkgeUvGvJGEJnPkIYZCkZ3BlyybYRgK+TwWnIYf4irNIrByGOScH3wtgGTFcdWEHRgpvY0nTjUjGG7gKYzB9nBKzQPlFS8wjixKHmABfGtrb1cYgwmiPm6FeQSGqpGLpMR8OYb6zAa31y8kQMDR6Gm/1PoU5i220tGk4OLwLI5MDSrS5tQtxzQW/w2Vzr1MrJpefwtHxd0iNVjLmGy5t624+2qvQCZu23lTEx64Hr/9bHzHbE6NEJN/ViUMNoh9W5oqWdbSkTDqM+Nhz9BXYtgM9jJFLcZiWg0PDb5EyVJYpcjct2Yxa8p5D1zPwb4yH+2BqydCqIcGl3PPY2tf6GJOxSyHo7lb3MJTb1LpSmHSCoGzOBcexouEuNDsXk3GJ/rPHsffMw/CjcUwW+1S88/4IjqQfxUjuNBrqGqEbGnTNwGcjfdg39AxqY4voHONJrjpRgN8rx6YxK+utzGjb3g1diTqzvZANaEMKTQ0WNrY9Bceu4wqGIPAxTHJ/NtaLdHFU8a1PzMa8WW1orp0HwzCZPvJuAbs/fhxT2iewtEY/lpSmmwm6Hlnzyg1lLCbyuYMGELji3rzwei0rZk4WDodrLnxSr4s10SooUEXTYcdjWBBfgvkti+nsVYoUHURBJ1SVcL7nYcqdgqSNcvGsdrx/bn+YMJvM/BSthyncx6A9dMrmO1+UUqWr++fdkg8KP7vxjXPfvHP1h4inNzVZV2lfv2hTpOuamJyawEv7/0yMqZDoFnlOZ2BeRJQwfkCguTRODfdiz6edaDDnIhFz0GC3RGfdAd3TB4Fi8taf3vz3A4zBp64yboVJ+UX5tLL5Zdz+8OUHXmxtXC7oAO6/uv95/dW+e7WG+BVojC/CnGSbWmbMwfUnMOb1IYtjCI0DuLz2meiaizbQ1ifNM+ljctexNet3rsdLZdtlLL5/gQBlvE4bTiiLckW2mP2IC81Y5iyefP0O1MVbfdp0RChczUeaRMhTwlEcjThss17aZiKiBJRFccb8ziWUfFoLb260BOsva2x2Pirbriagsr/6BbVVXUgX0judpMNLsLDv0Ds76ZQwYiU004hFhkV7qmM2i6R5iagx5ouE0SIoZzTCN2xHmFEghj881fVHmus6yRpMFcZ3TmMo29V4MwjQBIPPaxMTE3c7Nc5VvL9nc9nOTd/asmVp0z1LJ8ay389lgpe9vN/vB34hiHxKRI9zoFAs+Keyk/4/zo1k72sZX7/0e9f96IF8wf1rFJICNTVXnjx5cjPbZoxqApUQUAf1C3nw4EFz4cKFRxOJxIJsNusODQ0tWLZs2dnqSau3rDavXn5BM9Uf2hKBmK6n39w/NPLBjpl/wbq6uppaW1uPO45Tm8vlTtB3CdnyyljVNllqpcbg4OBXMplMQM+S2g/xICZFN/67Zuye/tM5Y/L0w27ZofMYHkvTlae9vb0P5PN52d/fH504cYKqmapyFeUrcnR2dio1TNOcTSU1ogn3E/tdTIyUUX8u+b/iNJZIpVIV9fgdPcuNovQPmp9pDjvBc589fPhwjsKwIwzDZurqL2PxuMpFg5VBYtnc09OzkDvYQGXA/9ko29i7d+/8I0eOMAG2WyH/H45a9ExgQQ3bAAAAAElFTkSuQmCC"},20625:function(e,t,n){var r=this&&this.__importDefault||function(e){return e&&e.__esModule?e:{default:e}};Object.defineProperty(t,"__esModule",{value:!0});var a=r(n(67294)),o=r(n(42628)),l=r(n(52263)),i=r(n(72389)),c=n(44256),s=r(n(25510)),u=r(n(14732)),d=r(n(74071)),m=[{names:["fbsource","fbs"],project:"fbsource",canonicalName:"fbsource"},{names:["www"],project:"facebook-www",canonicalName:"www"}];t.default=function(e){var t,n,r,p,f,h,g,b,y=(0,l.default)().siteConfig,A=(0,i.default)(),v=(0,o.default)(e);if(!A)return v;if("string"!=typeof e.file)return v;if((0,c.isInternal)()){if(!y.customFields)return v;var E=y.customFields,k=E.fbRepoName,C=E.ossRepoPath;if("string"!=typeof k)return v;t="string"==typeof C?function(){for(var e=arguments.length,t=new Array(e),n=0;n<e;n++)t[n]=arguments[n];return t.map((function(e){return e.startsWith("/")?e.slice(1):e})).map((function(e){return e.endsWith("/")?e.slice(0,e.length-1):e})).join("/")}(C,e.file):e.file;var N=m.find((function(e){return e.names.includes(k.toLowerCase())}));if(void 0===N)return v;n=function(e,t){var n=new URL("https://www.internalfb.com");return n.pathname="/code/"+e.canonicalName+"/"+t,n.toString()}(N,t),r=function(e,t){var n=new URL("https://www.internalfb.com/intern/nuclide/open/arc");return n.searchParams.append("project",e.project),n.searchParams.append("paths[0]",t),n.toString()}(N,t),p=function(e,t){if("fbsource"!==e.canonicalName||!t.startsWith("fbandroid"))return null;var n=new URL("fb-ide-opener://open");return n.searchParams.append("ide","intellij"),n.searchParams.append("filepath","/fbsource/"+t),n.toString()}(N,t)}else{if("string"!=typeof y.organizationName||"string"!=typeof y.projectName)return v;t=e.file,f=y.organizationName,h=y.projectName,g=e.file,(b=new URL("https://github.com")).pathname="/"+f+"/"+h+"/blob/master/"+g,n=b.toString(),r=null,p=null}var T=t.split("/"),w=T[T.length-1];return a.default.createElement("div",null,a.default.createElement("a",{href:n,title:"Browse entire file",target:"_blank",rel:"noreferrer",onClick:function(){return c.feedback.reportFeatureUsage({featureName:"browse-file",id:t})},className:d.default.CodeBlockFilenameTab},w),null!==r?a.default.createElement("a",{target:"_blank",rel:"noreferrer",href:r,onClick:function(){return c.feedback.reportFeatureUsage({featureName:"open-in-vscode",id:t})}},a.default.createElement("img",{style:{padding:"0 6px",height:"16px"},title:"Open in VSCode @ FB",src:s.default})):null,null!==p?a.default.createElement("a",{target:"_blank",rel:"noreferrer",href:p,onClick:function(){return c.feedback.reportFeatureUsage({featureName:"open-in-android-studio",id:t})}},a.default.createElement("img",{style:{padding:"0 6px",height:"16px"},title:"Open in Android Studio",src:u.default})):null,v)}},25510:function(e,t){Object.defineProperty(t,"__esModule",{value:!0});t.default="data:image/png;base64,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"},75854:function(e,t,n){var r=this&&this.__importDefault||function(e){return e&&e.__esModule?e:{default:e}};Object.defineProperty(t,"__esModule",{value:!0});var a=r(n(17642)),o=n(44256),l=Object.assign(Object.assign({},a.default),{FbInternalOnly:o.FbInternalOnly,FBInternalOnly:o.FbInternalOnly,OssOnly:o.OssOnly,OSSOnly:o.OssOnly});t.default=l},74071:function(e,t,n){n.r(t),t.default={CodeBlockFilenameTab:"CodeBlockFilenameTab_-TQn"}},23746:function(e,t,n){n.d(t,{ZP:function(){return h},lG:function(){return l}});var r=n(87410),a={plain:{backgroundColor:"#2a2734",color:"#9a86fd"},styles:[{types:["comment","prolog","doctype","cdata","punctuation"],style:{color:"#6c6783"}},{types:["namespace"],style:{opacity:.7}},{types:["tag","operator","number"],style:{color:"#e09142"}},{types:["property","function"],style:{color:"#9a86fd"}},{types:["tag-id","selector","atrule-id"],style:{color:"#eeebff"}},{types:["attr-name"],style:{color:"#c4b9fe"}},{types:["boolean","string","entity","url","attr-value","keyword","control","directive","unit","statement","regex","at-rule","placeholder","variable"],style:{color:"#ffcc99"}},{types:["deleted"],style:{textDecorationLine:"line-through"}},{types:["inserted"],style:{textDecorationLine:"underline"}},{types:["italic"],style:{fontStyle:"italic"}},{types:["important","bold"],style:{fontWeight:"bold"}},{types:["important"],style:{color:"#c4b9fe"}}]},o=n(67294),l={Prism:r.default,theme:a};function i(e,t,n){return t in e?Object.defineProperty(e,t,{value:n,enumerable:!0,configurable:!0,writable:!0}):e[t]=n,e}function c(){return c=Object.assign||function(e){for(var t=1;t<arguments.length;t++){var n=arguments[t];for(var r in n)Object.prototype.hasOwnProperty.call(n,r)&&(e[r]=n[r])}return e},c.apply(this,arguments)}var s=/\r\n|\r|\n/,u=function(e){0===e.length?e.push({types:["plain"],content:"\n",empty:!0}):1===e.length&&""===e[0].content&&(e[0].content="\n",e[0].empty=!0)},d=function(e,t){var n=e.length;return n>0&&e[n-1]===t?e:e.concat(t)},m=function(e,t){var n=e.plain,r=Object.create(null),a=e.styles.reduce((function(e,n){var r=n.languages,a=n.style;return r&&!r.includes(t)||n.types.forEach((function(t){var n=c({},e[t],a);e[t]=n})),e}),r);return a.root=n,a.plain=c({},n,{backgroundColor:null}),a};function p(e,t){var n={};for(var r in e)Object.prototype.hasOwnProperty.call(e,r)&&-1===t.indexOf(r)&&(n[r]=e[r]);return n}var f=function(e){function t(){for(var t=this,n=[],r=arguments.length;r--;)n[r]=arguments[r];e.apply(this,n),i(this,"getThemeDict",(function(e){if(void 0!==t.themeDict&&e.theme===t.prevTheme&&e.language===t.prevLanguage)return t.themeDict;t.prevTheme=e.theme,t.prevLanguage=e.language;var n=e.theme?m(e.theme,e.language):void 0;return t.themeDict=n})),i(this,"getLineProps",(function(e){var n=e.key,r=e.className,a=e.style,o=c({},p(e,["key","className","style","line"]),{className:"token-line",style:void 0,key:void 0}),l=t.getThemeDict(t.props);return void 0!==l&&(o.style=l.plain),void 0!==a&&(o.style=void 0!==o.style?c({},o.style,a):a),void 0!==n&&(o.key=n),r&&(o.className+=" "+r),o})),i(this,"getStyleForToken",(function(e){var n=e.types,r=e.empty,a=n.length,o=t.getThemeDict(t.props);if(void 0!==o){if(1===a&&"plain"===n[0])return r?{display:"inline-block"}:void 0;if(1===a&&!r)return o[n[0]];var l=r?{display:"inline-block"}:{},i=n.map((function(e){return o[e]}));return Object.assign.apply(Object,[l].concat(i))}})),i(this,"getTokenProps",(function(e){var n=e.key,r=e.className,a=e.style,o=e.token,l=c({},p(e,["key","className","style","token"]),{className:"token "+o.types.join(" "),children:o.content,style:t.getStyleForToken(o),key:void 0});return void 0!==a&&(l.style=void 0!==l.style?c({},l.style,a):a),void 0!==n&&(l.key=n),r&&(l.className+=" "+r),l})),i(this,"tokenize",(function(e,t,n,r){var a={code:t,grammar:n,language:r,tokens:[]};e.hooks.run("before-tokenize",a);var o=a.tokens=e.tokenize(a.code,a.grammar,a.language);return e.hooks.run("after-tokenize",a),o}))}return e&&(t.__proto__=e),t.prototype=Object.create(e&&e.prototype),t.prototype.constructor=t,t.prototype.render=function(){var e=this.props,t=e.Prism,n=e.language,r=e.code,a=e.children,o=this.getThemeDict(this.props),l=t.languages[n];return a({tokens:function(e){for(var t=[[]],n=[e],r=[0],a=[e.length],o=0,l=0,i=[],c=[i];l>-1;){for(;(o=r[l]++)<a[l];){var m=void 0,p=t[l],f=n[l][o];if("string"==typeof f?(p=l>0?p:["plain"],m=f):(p=d(p,f.type),f.alias&&(p=d(p,f.alias)),m=f.content),"string"==typeof m){var h=m.split(s),g=h.length;i.push({types:p,content:h[0]});for(var b=1;b<g;b++)u(i),c.push(i=[]),i.push({types:p,content:h[b]})}else l++,t.push(p),n.push(m),r.push(0),a.push(m.length)}l--,t.pop(),n.pop(),r.pop(),a.pop()}return u(i),c}(void 0!==l?this.tokenize(t,r,l,n):[r]),className:"prism-code language-"+n,style:void 0!==o?o.root:{},getLineProps:this.getLineProps,getTokenProps:this.getTokenProps})},t}(o.Component),h=f}}]);