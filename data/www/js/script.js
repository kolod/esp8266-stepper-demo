// Copyright 2020 (c) Aleksandr kolodki <alexandr.kolodkin@gmail.com>
// All rights reserved


function show_alert(message, msg_type = "danger") {

	// Delete all previous alert
	$(".alert").remove();

	// Create new alert
	$("#save").parent().after(`
		<div class="alert alert-${msg_type} alert-dismissible fade show" role="alert">
  			<strong>${message}</strong>
  			<button type="button" class="close" data-dismiss="alert" aria-label="Close">
    			<span aria-hidden="true">&times;</span>
  			</button>
		</div>
	`);
}

function scan_networks() {
	$.ajax({
		url: "scan",
		dataType: "json",
		timeout: 10000,
		success: function (output) {
			if (output.length > 0) {
				// Save selected option
				var ssid = $("#ssid option:selected").val();

				// Remove existing options
				$(".ssid_option").remove();

				// Add found netwoks
				for (ssid_info in output) {
					var html;
					if (output[ssid_info].ssid === ssid) {
						html = `<option class="ssid_option" selected>${output[ssid_info].ssid}</option>`;
					} else {
						html = `<option class="ssid_option">${output[ssid_info].ssid}</option>`;
					}
					$("#ssid").append(html);
				}

			} else {
				setTimeout(scan_networks, 10000);
			}
		},
		error: function(xmlhttprequest, textstatus, message) {
			setTimeout(scan_networks, 10000);
		}
	});
}

$(document).ready(function () {

	scan_networks();

	$("#move90cw").on("click", function (e) {
	   e.preventDefault();
	   $.ajax({
		   url: "stepper",
		   data: {
			   "mode": "movecw",
			   "value": 90
		   }
	   });
	});

	
	$("#move90ccw").on("click", function (e) {
		e.preventDefault();
		$.ajax({
			url: "stepper",
			data: {
				"mode": "moveccw",
				"value": 90
			}
		});
	});

	 
	$("#move180cw").on("click", function (e) {
		e.preventDefault();
		$.ajax({
			url: "stepper",
			data: {
				"mode": "movecw",
				"value": 180
			}
		});
	});

	 
	$("#move180ccw").on("click", function (e) {
		e.preventDefault();
		$.ajax({
			url: "stepper",
			data: {
				"mode": "moveccw",
				"value": 180
			}
		});
	});

	 
	$("#move360cw").on("click", function (e) {
		e.preventDefault();
		$.ajax({
			url: "stepper",
			data: {
				"mode": "movecw",
				"value": 360
			}
		});
	});

	 
	$("#move360ccw").on("click", function (e) {
		e.preventDefault();
		$.ajax({
			url: "stepper",
			data: {
				"mode": "moveccw",
				"value": 360
			}
		});
	});

	// Request networs
	$("#scan").on("click", function (e) {
		e.preventDefault();
		scan_networks();
	});

	// Request reboot
	$("#reboot").on("click", function (e) {
		e.preventDefault();
		$.ajax({
			url: "reboot",
			dataType: "json",
			timeout: 10000,
			success: function (output) {
				// Show alert if requst complited
				if (typeof(output.result) === 'undefined') {
					show_alert("Settings isn't saved. Unknown error.");
				} else {
					if (output.result == 'Ok') {
						show_alert("Settings saved successfully.", "success");
					}
				}
			},
			error: function(xmlhttprequest, textstatus, message) {
				if (textstatus === "error") {
					show_alert("Settings isn't saved. Server error: " + message + ".");
				} else if (textstatus === "timeout") {
					show_alert("Settings isn't saved. Server not respond.");
				}
			}
		});
	});

	// SSID selection changed
	$("#ssid").change(function (){
		if ($("#ssid option:selected").val() == $("#manual-ssid-option").val()) {
			$("#manual-ssid").show().focus();
		} else {
			$("#manual-ssid").hide();
		}
	});
	
	// Save new network SSID & password
	$("#save").on("click", function (e) {
		e.preventDefault();

		// SSID
		var ssid = $("#ssid option:selected").text();
		if (ssid == $("#manual-ssid-option").text()) ssid = $("#manual-ssid").val();

		// Password
		var pass = $("#wifi-password").val();

		if (ssid == "") {
			// Show alert if SSID is empty
			show_alert("SSID must be filled out!");
		} else if (pass == "") {
			// Show alert if password is empty
			show_alert("Password must be filled out!");
		} else {

			// Send request
			$.ajax({
				url: "save",
				data: {
					ssid: ssid,
					pass: pass,
				},
				dataType: "json",
				timeout: 10000,
				success: function (output) {
					// Show alert if requst complited
					if (typeof(output.result) === 'undefined') {
						show_alert("Settings isn't saved. Unknown error.");
					} else {
						if (output.result == 'Ok') {
							show_alert("Settings saved successfully.", "success");
						} else {
							show_alert("Settings isn't saved. " + output.error);
						}
					}
				},
				error: function(xmlhttprequest, textstatus, message) {
					if (textstatus === "error") {
						show_alert("Settings isn't saved. Server error: " + message + ".");
					} else if (textstatus === "timeout") {
						show_alert("Settings isn't saved. Server not respond.");
					}
				}
			});
		}
	});
});