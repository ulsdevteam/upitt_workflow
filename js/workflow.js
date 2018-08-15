/**
 * @file
 * Javascript file for pitt_workflow
 */

jQuery( document ).ready(function() {
  var batch_host = jQuery('#edit-batch-host').val();
  if (batch_host == 'ftp') {
    var protocol = window.location.protocol;
    var hostname = window.location.host;
    host_selected('ftp', protocol + '//' + hostname);
  }
});

function host_selected(control, site_url) {
  var ftp_selected = (control == 'ftp' || control.value == 'ftp');
  if (ftp_selected) {
    jQuery('#edit-local-files').hide();
    jQuery('#edit-box-files-wrapper').show();
//    jQuery('#edit-box-files').show();
    var current_path = jQuery('[name="initial_ftp_path"]').val();
    current_path = current_path.replace("ftp:", "");
    ftp_change_path(current_path, site_url);
  }
  else {
    jQuery('#edit-box-files-wrapper').hide();
//    jQuery('#edit-box-files').hide();
    jQuery('#edit-local-files').show();
  } 
}

function ftp_change_path(path, site_url) {
  if (!path.includes("Digital Collections Contributors")) {
    path = 'Digital Collections Contributors';
  }
  var get_ftp_files_url = site_url + '/ajax/workflow/browse_ftp/' + path;
  jQuery('#box-files-output').remove();
  jQuery('#edit-box-files').html('<div id="edit-box-files-loading-img"><h3>Loading results from FTP</h3><p>Please be patient...</p></div>');
  var ajaxcall = jQuery.get(get_ftp_files_url, function( data ) {
    jQuery('#edit-box-files').html( data );
    path = path.replace("ftp:", "");
    jQuery('[name="initial_ftp_path"]').val('ftp:' + path);
    jQuery('#edit-box-files-wrapper').show();
//    jQuery('#edit-box-files').show();
  });
}

function processing_mode_selected(control) {
    if ((control.value === 'gen_notimelimit_OCR') || (control.value === 'gen_OCR')) {
        jQuery('#edit-language-options').show();
        jQuery('#edit-language-options').removeClass("display_none");
    }
    else {
        jQuery('#edit-language-options').hide();
        jQuery('#edit-language-options').addClass("display_none");
    }
}
