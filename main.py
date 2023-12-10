from discord import *
import os
import datetime
from discord.ext import commands
from dotenv import load_dotenv

intents = Intents.all()
client = commands.Bot(command_prefix='?', intents=intents)

load_dotenv()
TOKEN = os.getenv('DISCORD_TOKEN')

@client.event
async def on_ready() -> None:
    print("We have logged in as {0.user}".format(client))


@client.event
async def on_typing(channel: abc.Messageable, user: [Member, User], when: datetime.datetime) -> None:
    pass


@client.command()
async def imp(ctx: commands.context.Context) -> None:
    await ctx.message.delete()
    await ctx.send('Imp!')


@client.command()
async def iroha(ctx: commands.context.Context) -> None:
    author = ctx.message.author
    await ctx.message.delete()
    await author.send(file=File("iroha.jpg"))
    await author.send("Get Iroha'd")


client.run(TOKEN)
