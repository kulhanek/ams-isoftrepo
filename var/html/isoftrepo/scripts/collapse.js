// taken from
// http://stackoverflow.com/questions/1933602/how-to-getelementbyclass-instead-of-getelementbyid-with-javascript

function getElementsByClassName(node,classname) {
  if (node.getElementsByClassName) { // use native implementation if available
    return node.getElementsByClassName(classname);
  } else {
    return (function getElementsByClass(searchClass,node) {
        if ( node == null )
          node = document;
        var classElements = [],
            els = node.getElementsByTagName("*"),
            elsLen = els.length,
            pattern = new RegExp("(^|\\s)"+searchClass+"(\\s|$)"), i, j;

        for (i = 0, j = 0; i < elsLen; i++) {
          if ( pattern.test(els[i].className) ) {
              classElements[j] = els[i];
              j++;
          }
        }
        return classElements;
    })(classname, node);
  }
}

// ------------------------

function toggle_visibility() {

    var checked = document.getElementById("show_all").checked;

    var className = "old";
    var elements = getElementsByClassName(document, className);
    var n = elements.length;
    for (var i = 0; i < n; i++) {
        var e = elements[i];

        if( checked ) {
            e.style.display = 'list-item';
        } else {
            e.style.display = 'none';
        }
    }

    className = "switch";
    elements = getElementsByClassName(document, className);
    n = elements.length;
    for (i = 0; i < n; i++) {
        e = elements[i];
        if( checked ) {
            e.style.display = 'none';
        } else {
            e.style.display = 'list-item';
        }
    }
}
