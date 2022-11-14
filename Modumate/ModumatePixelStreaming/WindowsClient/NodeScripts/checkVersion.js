const request = require('request');
const fs = require('fs');
const http = require('http');
const unzipper = require('unzipper');
const childProcess = require('child_process');


function unzipFile()
{
	console.log('unzipping latest');
	fs.rm('d:/modumate/pixelStreamingClient',{recursive: true},err=>{});
	var stream = fs.createReadStream('d:/Modumate/pixelStreamingClient.zip').pipe(unzipper.Extract({path:'d:/Modumate/pixelStreamingClient'}));
	console.log('done');
}

function downloadLatest()
{
	const clientURL = 'http://modumate-latest.s3.us-west-1.amazonaws.com/internal/pixelStreaming/PixelStreamingClient.zip';
	
	console.log('downloading latest');
	const file = fs.createWriteStream('d:/modumate/PixelStreamingClient.zip');
	const req = http.get(clientURL, function(response)
	{
		response.pipe(file);

		file.on("finish",() => 
		{  
			file.close();
			unzipFile();
		});
	});
}

function onLatestVersionReceived(version)
{
	const versionFile = 'd:/Modumate/pixelStreamingClient/PixelStreamingClient/build_version.txt';
	fs.readFile(versionFile,'utf8',(err,data) => {
		if (err)
		{
			console.log('no version found');
			downloadLatest();
			return;
		}
		if (version != data.trim())
		{
			console.log('version out of date');
			downloadLatest();
			return;
		}
		console.log('version up to date');
	});
}

console.log('Checking Version');
request('https://account.modumate.com/api/v2/meta/client/version', {json:true}, (err,res,body) => {
	if (err) {return console.log(err);}
	onLatestVersionReceived(body.version.trim());
});

return;
