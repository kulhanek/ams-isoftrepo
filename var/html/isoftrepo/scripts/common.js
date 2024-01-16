function do_action(act)
{
 document.forms[0].action.value=act;
 document.forms[0].submit();
}

function do_submit()
{
 if( document.forms[0].action.value == "none" ){
    document.forms[0].action.value = "search";
    }
}

function do_load()
{
 document.forms[0].search.focus();
}

