/**
 * @file
 * Javascript file for islandora solr extras
 */

function host_selected(control, site_url) {
  var ftp_selected = (control.value == 'ftp');
  var initial_path = jQuery('#edit-initial-batch-host').val();
  jQuery('#edit-box-selector option').remove();

  if (ftp_selected) {
    jQuery('#edit-local-files').hide();
    jQuery('#edit-box-files').show();
  }
  else {
    jQuery('#edit-box-files').hide();
    jQuery('#edit-local-files').show();
  } 
  var current_path = jQuery('[name="initial_ftp_path"]').val();
  current_path = current_path.replace("/", "|");
  
  var get_ftp_files_url = site_url + '/ajax/workflow/browse_ftp/' + current_path;
  console.log(get_ftp_files_url);
  
  var ajaxcall = jQuery.get(get_ftp_files_url, function( data ) {
    jQuery('#edit-box-files').html( data );
  });
          
}
